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

#if WITH_GAMEPLAY_DEBUGGER
			virtual FString Debug_GetName() const override { return TEXT("Wait for event"); }
#endif
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
					const EStatus ReceiveStatus = HandleValues(Values...);
					if (FCoroutineExecutor::IsFinished(ReceiveStatus))
					{
						CachedExec->ForceNodeEnd(this, ReceiveStatus);
					}
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

	template<typename ...TValues>
	using TEventPtr = TSharedPtr<TEvent<TValues...>, DefaultSPMode>;

	template<typename ...TValues>
	using TEventWeakPtr = TWeakPtr<TEvent<TValues...>, DefaultSPMode>;

	namespace Detail
	{
		template<typename TLambda, typename TLambdaRetValue, typename ...TValues>
		class TEventListener : public TEventListenerBase<TValues...>
		{
		};

		template<typename TLambda, typename ...TValues>
		class TEventListener<TLambda, void, TValues...> : public TEventListenerBase<TValues...>
		{
		public:
			TEventListener(TEventRef<TValues...> const& _Event, TLambda const& _Lambda)
				:TEventListenerBase<TValues...>(_Event)
				,Lambda(_Lambda)
			{}
			virtual EStatus HandleValues(TValues const&... Values) override
			{
				Lambda(Values...);
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
				return Lambda(Values...) ? Completed : Failed;
			}
			TLambda Lambda;
		};

		template<typename TLambda, typename ...TValues>
		class TEventListener<TLambda, FCoroutineNodeRef, TValues...> : public TEventListenerBase<TValues...>
		{
		public:
			TEventListener(TEventRef<TValues...> const& _Event, TLambda const& _Lambda)
				:TEventListenerBase<TValues...>(_Event)
				,Lambda(_Lambda)
			{}
			//for bool return values we use it to determine whether this listener simply completes or propagates a failure
			virtual EStatus HandleValues(TValues const&... Values) override
			{
				if (this->CachedExec)
				{
					Child = Lambda(Values...);
					this->CachedExec->EnqueueCoroutineNode(Child.ToSharedRef(), this);
				}
				return Suspended;
			}
			//We're standing in for the child coroutine, so we replicate its end status
			virtual EStatus OnChildStopped(FCoroutineExecutor*, EStatus Status, FCoroutineNode*) override { return Status; }
			virtual void End(FCoroutineExecutor* Executor, EStatus Status) override
			{
				if (Status == Aborted)
				{
					Executor->AbortNode(Child.ToSharedRef());
				}
				Child.Reset(); //Child has finished its execution, so it can be released
			};
			TLambda Lambda;
			FCoroutineNodePtr Child;
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
		class TEventListener<TLambda, FCoroutineNodeRef, void> : public FEventListenerBase
		{
		public:
			TEventListener(TEventRef<void> const& _Event, TLambda const& _Lambda)
				:FEventListenerBase(_Event)
				,Lambda(_Lambda)
			{}
			virtual void ReceiveEvent() override
			{
				if (this->CachedExec)
				{
					Child = Lambda();
					this->CachedExec->EnqueueCoroutineNode(Child.ToSharedRef(), this);
				}
			}
			//We're standing in for the child coroutine, so we replicate its end status
			virtual EStatus OnChildStopped(FCoroutineExecutor*, EStatus Status, FCoroutineNode*) override { return Status; }
			virtual void End(FCoroutineExecutor* Executor, EStatus Status) override
			{
				if (Status == Aborted)
				{
					Executor->AbortNode(Child.ToSharedRef());
				}
				Child.Reset(); //Child has finished its execution, so it can be released
			};
			TLambda Lambda;
			FCoroutineNodePtr Child;
		};
	}

	//Suspends execution until the event is broadcast, passes the broadcast parameters (if any) to the lambda
	template <typename TLambda, typename ...TValues>
	FCoroutineNodeRef _WaitFor(TEventRef<TValues...> const& Event, TLambda const& Lambda)
	{
		typedef typename ::TFunctionTraits<decltype(&TLambda::operator())>::RetType LambdaRetType;
		static_assert(TFunctionTraits<decltype(&TLambda::operator())>::ArgCount == 0
			|| std::is_same_v<TTuple<TValues...>, typename TFunctionTraits<decltype(&TLambda::operator())>::ArgTypes>,
			"Lambdas in _WaitFor must match argument types exactly. They can't use implicit conversions");
		static_assert(std::is_void_v<LambdaRetType>
			|| std::is_same_v<bool, LambdaRetType>
			|| std::is_same_v<FCoroutineNodeRef, LambdaRetType>,
			"EventListeners only support void, bool, and FCoroutineNodeRef return types");
		return MakeShared<Detail::TEventListener<TLambda, LambdaRetType, TValues...>, DefaultSPMode>(Event, Lambda);
	}

	//Suspends execution until the event is broadcast
	FCoroutineNodeRef ACETEAM_COROUTINES_API _WaitFor(TEventRef<void> const& Event);

	//Makes a shared event that can be listened to by a coroutine node. Can be broadcast from any system that holds a reference
	//Will broadcast any values passed in as parameters to its Broadcast() function
	template <typename ...TValues>
	TEventRef<TValues...> MakeEvent()
	{
		return MakeShared<TEvent<TValues...>, DefaultSPMode>();
	}

	//Makes a shared event that can be listened to by a coroutine node. Can be broadcast from any system that holds a reference
	//This version does not receive any values to Broadcast()
	inline TEventRef<void> MakeEvent()
	{
		return MakeShared<TEvent<void>, DefaultSPMode>();
	}
}
