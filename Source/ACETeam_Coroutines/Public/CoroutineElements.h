// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "CoroutineNode.h"
#include "FunctionTraits.h"

namespace ACETeam_Coroutines
{
	namespace Detail
	{
		class ACETEAM_COROUTINES_API FCoroutineDecorator : public FCoroutineNode
		{
		protected:
			FCoroutineNodePtr m_Child;
		public:
			// Start is normal behavior for decorators, but not forced
			virtual EStatus Start(FCoroutineExecutor* Executor) override;
			virtual void AddChild(FCoroutineNodePtr const& Child)
			{
				if (m_Child)
				{
					//UE_LOG(LogACETeam_Coroutines, Error, TEXT("Trying to give a decorator more than one child"));
					UE_DEBUG_BREAK();
				}
				m_Child = Child; 
			}
			virtual void End(FCoroutineExecutor* Executor, EStatus Status) override;
			virtual int GetNumChildren() { return m_Child ? 1 : 0; }
		};

		// Launch a task to run independent of its launching context
		class ACETEAM_COROUTINES_API FFork : public FCoroutineDecorator
		{
		public:
			virtual EStatus Start(FCoroutineExecutor* Exec) override;
		};

		class ACETEAM_COROUTINES_API FLoop : public FCoroutineDecorator
		{
			int m_LastOpen;
		
		public:
			virtual EStatus Start(FCoroutineExecutor* Exec) override;
			virtual EStatus Update(FCoroutineExecutor* Exec, float) override;
			virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode*) override;
		};

		template<typename TScopeEndedLambda>
		class TScope : public FCoroutineDecorator
		{
			TScopeEndedLambda m_OnScopeEnd;
		
		public:
			TScope(TScopeEndedLambda& OnScopeEnd) : m_OnScopeEnd(OnScopeEnd){}
			virtual EStatus OnChildStopped(FCoroutineExecutor*, EStatus Status, FCoroutineNode*) override
			{
				m_OnScopeEnd();
				return Status;
			}
		};

		class ACETEAM_COROUTINES_API FCompositeCoroutine : public FCoroutineNode
		{
		protected:
			typedef TArray<FCoroutineNodePtr> FChildren;
			FChildren m_Children;
		public:
			virtual void AddChild(FCoroutineNodePtr const& Child) ;
			virtual void End(FCoroutineExecutor* Exec, EStatus Status) override;
			virtual int GetNumChildren() { return m_Children.Num(); }
		};

		class ACETEAM_COROUTINES_API FSequence final : public FCompositeCoroutine
		{
			uint32 m_CurChild = 0;
		public:
			virtual EStatus Start(FCoroutineExecutor* Exec) override;
			virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child) override;
		};

		class ACETEAM_COROUTINES_API FTimer : public FCoroutineNode
		{
			float m_Timer;
			float m_TargetTime;
		public:
			FTimer(float TargetTime): m_TargetTime (TargetTime) {}
			virtual EStatus Start(FCoroutineExecutor*) override
			{
				m_Timer = m_TargetTime;
				return Running; 
			}
			virtual EStatus Update(FCoroutineExecutor*, float DeltaTime) override
			{
				m_Timer -= DeltaTime;
				if (m_Timer <= 0.0f)
					return Completed;
				return Running;
			}
		};

		class ACETEAM_COROUTINES_API FFrameTimer : public FCoroutineNode
		{
			int m_Frames;
			int m_TargetFrames;
		public:
			FFrameTimer(int TargetFrames): m_TargetFrames (TargetFrames) {}
			virtual EStatus Start(FCoroutineExecutor*) override
			{
				m_Frames = m_TargetFrames+1;
				return Running;
			}
			virtual EStatus Update(FCoroutineExecutor*, float) override
			{
				--m_Frames;
				if (m_Frames <= 0)
					return Completed;
				return Running;
			}
		};

		class ACETEAM_COROUTINES_API FParallelBase : public FCompositeCoroutine
		{
		public:
			virtual EStatus Start(FCoroutineExecutor* Exec) override;
			void AbortOtherBranches( FCoroutineNode* Child, FCoroutineExecutor* Exec );
		};

		class ACETEAM_COROUTINES_API FRace : public FParallelBase
		{
		public:
			virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child) override;
		};

		class ACETEAM_COROUTINES_API FSync : public FParallelBase
		{
			uint32 m_ClosedCount = 0;
			EStatus m_EndStatus = Completed;
		public:
			virtual EStatus Start(FCoroutineExecutor* Exec) override;
			virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child) override;
		};

		template<typename TCoroutine>
		inline void AddCoroutineChild(TSharedRef<TCoroutine>& Composite, FCoroutineNodePtr& First)
		{
			Composite->AddChild(First);
		}

		template <typename TCoroutine, typename TLambda, typename TLambdaRetType = void>
		struct TAddCompositeChildHelper
		{
			void operator()(TSharedRef<TCoroutine>& Composite, TLambda& First)
			{
				Composite->AddChild(_Lambda(First));
			}
		};

		template <typename TCoroutine, typename TLambda>
		struct TAddCompositeChildHelper<TCoroutine, TLambda, bool>
		{
			void operator()(TSharedRef<TCoroutine>& Composite, TLambda& First)
			{
				Composite->AddChild(_ConditionalLambda(First));
			}
		};

		template <typename TCoroutine, typename TLambda>
		struct TAddCompositeChildHelper<TCoroutine, TLambda, FCoroutineNodePtr>
		{
			void operator()(TSharedRef<TCoroutine>& Composite, TLambda& First)
			{
				Composite->AddChild(_Deferred(First));
			}
		};

		template <typename TCoroutine, typename TLambda>
		void AddCoroutineChild(TSharedRef<TCoroutine>& Composite, TLambda& First)
		{
			TAddCompositeChildHelper<TCoroutine, TLambda, typename ::TFunctionTraits<decltype(&TLambda::operator())>::RetType>()(Composite, First);
		}

		template <typename TChild>
		void AddCompositeChildren(TSharedRef<FCompositeCoroutine>& Composite, TChild& Child)
		{
			AddCoroutineChild(Composite, Child);
		}
		
		template<typename TChild, typename... TChildren>
		void AddCompositeChildren(TSharedRef<FCompositeCoroutine>& Composite, TChild& First, TChildren... Children)
		{
			AddCoroutineChild(Composite, First);
			AddCompositeChildren(Composite, Children...);
		}
		
		template<typename TComposite, typename... TChildren>
		FCoroutineNodePtr MakeComposite(TChildren... Children)
		{
			auto Comp = StaticCastSharedRef<FCompositeCoroutine>(MakeShared<TComposite>());
			AddCompositeChildren(Comp, Children...);
			return Comp;
		}

		template <typename TScopeLambda>
		struct TScopeHelper
		{
			TScopeLambda m_ScopeLambda;

			TScopeHelper(TScopeLambda& ScopeLambda) : m_ScopeLambda(ScopeLambda) {}

			template <typename TChild>
			FCoroutineNodePtr operator() (TChild&& ScopeBody)
			{
				auto Scope = MakeShared<TScope<TScopeLambda>>(m_ScopeLambda);
				AddCoroutineChild(Scope, ScopeBody);
				return Scope;
			}
		};
	}

	template<typename ...TChildren>
	FCoroutineNodePtr _Seq(TChildren... Children)
	{
		return Detail::MakeComposite<Detail::FSequence>(Children...);
	}

	template<typename ...TChildren>
	FCoroutineNodePtr _Race(TChildren... Children)
	{
		return Detail::MakeComposite<Detail::FRace>(Children...);
	}

	template<typename ...TChildren>
	FCoroutineNodePtr _Sync(TChildren... Children)
	{
		return Detail::MakeComposite<Detail::FSync>(Children...);
	}

	inline FCoroutineNodePtr _Wait(float Time)
	{
		return MakeShared<Detail::FTimer>(Time);
	}

	inline FCoroutineNodePtr _WaitFrames(int Frames)
	{
		return MakeShared<Detail::FFrameTimer>(Frames);
	}

	template<typename TChild>
	FCoroutineNodePtr _Loop(TChild Body)
	{
		auto Loop = MakeShared<Detail::FLoop>();
		Detail::AddCoroutineChild(Loop, Body);
		return Loop;
	}

	template<typename TOnScopeExit>
	Detail::TScopeHelper<TOnScopeExit> _Scope(TOnScopeExit&& OnScopeExit)
	{
		return Detail::TScopeHelper<TOnScopeExit>(OnScopeExit);
	}

	template<typename TChild>
	FCoroutineNodePtr _Fork(TChild Body)
	{
		auto Fork = MakeShared<Detail::FFork>();
		Detail::AddCoroutineChild(Fork, Body);
		return Fork;
	}
}