// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "CoroutineExecutor.h"
#include "CoroutineNode.h"
#include "FunctionTraits.h"

namespace ACETeam_Coroutines
{
	namespace Detail
	{
		class FEventListenerBase;
		typedef TSharedRef<FEventListenerBase, DefaultSPMode> FEventListenerRef;
		
		class ACETEAM_COROUTINES_API FEventBase
		{
		public:
			void AddListener(FEventListenerRef const& Listener);
			void RemoveListener(FEventListenerRef const& Listener);
			void AbortListeners();
		protected:
			TArray<FEventListenerRef, TInlineAllocator<1>> Listeners;
		};
		
		class ACETEAM_COROUTINES_API FEventListenerBase : public FCoroutineNode, public TSharedFromThis<FEventListenerBase, DefaultSPMode>
		{
			virtual EStatus Start(FCoroutineExecutor* Exec) override;

			virtual void End(FCoroutineExecutor* Exec, EStatus Status) override;
		public:
			FEventListenerBase(TSharedRef<FEventBase, DefaultSPMode> const& _Event)
			:Event(_Event)
			{}
			virtual void ReceiveEvent();
			void EventAborted();
		protected:
			FCoroutineExecutor* CachedExec = nullptr;
			TSharedRef<FEventBase, DefaultSPMode> Event;
		};

		template <typename ...TValues>
		class TEventListenerBase : public FEventListenerBase
		{
		public:
			TEventListenerBase(TSharedRef<FEventBase, DefaultSPMode> const& _Event) : FEventListenerBase(_Event) {}
			void ReceiveEventValues(TValues const&... Values)
			{
				if (CachedExec)
				{
					CachedExec->ForceNodeEnd(this, HandleValues(Values...));
				}
			}
		protected:
			virtual EStatus HandleValues(TValues const&... Values) = 0;
		};
		template <>
		class TEventListenerBase<void> : public FEventListenerBase
		{
			TEventListenerBase<void>()=delete;
		};
		template <typename ...TValues>
		using TEventListenerRef = TSharedRef<TEventListenerBase<TValues...>, DefaultSPMode>;
	}
	
	template<typename ...TValues>
	class TEvent : public Detail::FEventBase
	{
	public:
		void Broadcast(TValues... Values)
		{
			using namespace Detail;
			check(IsInGameThread());
			static_assert(TIsDerivedFrom<typename TEventListenerRef<TValues...>::ElementType, FEventListenerBase>::Value, "reinterpret_cast is not safe");
#if ENGINE_MAJOR_VERSION >= 5
			typedef TArray<TEventListenerRef<TValues...>, typename decltype(Listeners)::AllocatorType> TListeners;
#else
			typedef TArray<TEventListenerRef<TValues...>, typename decltype(Listeners)::Allocator> TListeners;
#endif		
			auto TempListeners = MoveTemp(reinterpret_cast<TListeners&>(Listeners));
			for (auto& Listener : TempListeners)
			{
				Listener->ReceiveEventValues(Values...);
			}
		}
	};
	
	template<>
	class TEvent<void> : public Detail::FEventBase
	{
	public:		
		void Broadcast()
		{
			using namespace Detail;
			check(IsInGameThread());
			static_assert(TIsDerivedFrom<typename TEventListenerRef<>::ElementType, FEventListenerBase>::Value, "reinterpret_cast is not safe");
#if ENGINE_MAJOR_VERSION >= 5
			typedef TArray<TEventListenerRef<void>, decltype(Listeners)::AllocatorType> TListeners;
#else
			typedef TArray<TEventListenerRef<void>, decltype(Listeners)::Allocator> TListeners;
#endif		
			auto TempListeners = MoveTemp(reinterpret_cast<TListeners&>(Listeners));
			for (const auto& Listener : TempListeners)
			{
				Listener->ReceiveEvent();
			}
		}
	};

	template<typename ...TValues>
	using TEventRef = TSharedRef<TEvent<TValues...>, DefaultSPMode>;

	namespace Detail
	{
		template<typename TLambda, typename TLambdaRetValue, typename ...TValues>
		class TEventListener : public TEventListenerBase<TValues...>
		{
		public:
			TEventListener(TEventRef<TValues...> const& _Event, TLambda const& _Lambda)
				:TEventListenerBase<TValues...>(_Event)
				,Lambda(_Lambda)
			{}
			//by default we ignore the lambda's return value (if any)
			virtual EStatus HandleValues(TValues const&... Values) override
			{
				static_assert(TIsSame<TTuple<TValues...>, typename TFunctionTraits<decltype(&TLambda::operator())>::ArgTypes>::Value, "Lambdas in _WaitFor can't use implicit conversions");
				(void)Lambda(Values...);
				return Completed;
			}
			TLambda Lambda;
		};
		
		template<typename TLambda, typename ...TValues>
		class TEventListener<TLambda, bool, TValues...> : public TEventListenerBase<TValues...>
		{
		public:
			TEventListener(TEventRef<TValues...> const& _Event, TLambda const& _Lambda)
				:TEventListenerBase<TValues...>(_Event)
				,Lambda(_Lambda)
			{}
			//for bool return values we use it to determine whether this listener simply completes or propagates a failure
			virtual EStatus HandleValues(TValues const&... Values) override
			{
				static_assert(TIsSame<TTuple<TValues...>, typename TFunctionTraits<decltype(&TLambda::operator())>::ArgTypes>::Value, "Lambdas in _WaitFor can't use implicit conversions");
				return Lambda(Values...) ? Completed : Failed;
			}
			TLambda Lambda;
		};

		template<typename TLambda>
		class TEventListener<TLambda, bool, void> : public FEventListenerBase
		{
		public:
			TEventListener(TEventRef<void> const& _Event, TLambda const& _Lambda)
				:FEventListenerBase(_Event)
				,Lambda(_Lambda)
			{}
			virtual void ReceiveEvent() override
			{
				if (CachedExec)
				{
					CachedExec->ForceNodeEnd(this, Lambda() ? Completed : Failed);
				}
			}
			TLambda Lambda;
		};

		template<typename TLambda>
		class TEventListener<TLambda, void, void> : public FEventListenerBase
		{
		public:
			TEventListener(TEventRef<void> const& _Event, TLambda const& _Lambda)
				:FEventListenerBase(_Event)
				,Lambda(_Lambda)
			{}
			virtual void ReceiveEvent() override
			{
				Lambda();
				FEventListenerBase::ReceiveEvent();
			}
			TLambda Lambda;
		};
	}

	template <typename TLambda, typename ...TValues>
	FCoroutineNodeRef _WaitFor(TEventRef<TValues...> const& Event, TLambda const& Lambda)
	{
		typedef typename ::TFunctionTraits<decltype(&TLambda::operator())>::RetType LambdaRetType;
		return MakeShared<Detail::TEventListener<TLambda, LambdaRetType, TValues...>, DefaultSPMode>(Event, Lambda);
	}
	
	FCoroutineNodeRef ACETEAM_COROUTINES_API _WaitFor(TEventRef<void> const& Event);

	template <typename ...TValues>
	TEventRef<TValues...> MakeEvent()
	{
		return MakeShared<TEvent<TValues...>, DefaultSPMode>();
	}

	inline TEventRef<void> MakeEvent()
	{
		return MakeShared<TEvent<void>, DefaultSPMode>();
	}
}
