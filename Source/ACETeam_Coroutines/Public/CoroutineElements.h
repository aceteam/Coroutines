// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "CoroutineExecutor.h"
#include "CoroutineNode.h"
#include "FunctionTraits.h"

namespace ACETeam_Coroutines
{
	namespace Detail
	{
		template <typename TLambda>
		class TLambdaCoroutine: public FCoroutineNode
		{
			TLambda	m_Lambda;
		public:
			TLambdaCoroutine (TLambda const & Lambda) : m_Lambda(Lambda){}
			virtual EStatus Start(FCoroutineExecutor*) override { m_Lambda(); return Completed;}
		};

		template <typename TLambda>
		class TWeakLambdaCoroutine: public FCoroutineNode
		{
			FWeakObjectPtr m_Object;
			TLambda	m_Lambda;
		public:
			TWeakLambdaCoroutine (UObject* Obj, TLambda const & Lambda)
				: m_Object(Obj)
				, m_Lambda(Lambda){}
			virtual EStatus Start(FCoroutineExecutor*) override
			{
				if (m_Object.IsValid())
				{
					m_Lambda();
				}
				return Completed;
			}
		};

		template <typename TLambda>
		class TConditionLambdaCoroutine : public FCoroutineNode
		{
			TLambda m_Lambda;
		public:
			TConditionLambdaCoroutine (TLambda const & Lambda) : m_Lambda(Lambda) {}
			virtual EStatus Start(FCoroutineExecutor*) override { return m_Lambda() ? Completed : Failed; }
		};

		template <typename TLambda>
		class TWeakConditionLambdaCoroutine: public FCoroutineNode
		{
			FWeakObjectPtr m_Object;
			TLambda	m_Lambda;
		public:
			TWeakConditionLambdaCoroutine (UObject* Obj, TLambda const & Lambda)
				: m_Object(Obj)
				, m_Lambda(Lambda){}
			virtual EStatus Start(FCoroutineExecutor*) override
			{
				if (m_Object.IsValid())
				{
					return m_Lambda() ? Completed : Failed;
				}
				return Failed;
			}
		};
		
		template <typename TLambda>
		class TDeferredCoroutineWrapper : public FCoroutineNode
		{
			TLambda m_Lambda;
			FCoroutineNodePtr m_Child;
		public:
			TDeferredCoroutineWrapper (TLambda const& Lambda) : m_Lambda(Lambda) {}
			virtual EStatus Start(FCoroutineExecutor* Executor) override
			{
				m_Child = m_Lambda();
				Executor->EnqueueCoroutineNode(m_Child.ToSharedRef(),this);
				return Suspended; 
			}
			
			//We're standing in for the child coroutine, so we replicate its end status
			virtual EStatus OnChildStopped(FCoroutineExecutor*, EStatus Status, FCoroutineNode*) override { return Status; }
			virtual void End(FCoroutineExecutor* Executor, EStatus Status) override
			{
				if (Status == Aborted)
				{
					Executor->AbortNode(m_Child.ToSharedRef());
				}
				m_Child.Reset(); //Child has finished its execution, so it can be released
			};
		};

		template <typename TLambda>
		class TWeakDeferredCoroutineWrapper : public TDeferredCoroutineWrapper<TLambda>
		{
			FWeakObjectPtr m_Object;
			TWeakDeferredCoroutineWrapper (UObject* Obj, TLambda const& Lambda)
			: TDeferredCoroutineWrapper(Lambda)
			, m_Object(Obj){}

			virtual EStatus Start(FCoroutineExecutor* Executor) override
			{
				if (m_Object.IsValid())
				{
					return TDeferredCoroutineWrapper<TLambda>::Start(Executor);
				}
				return Completed;
			}
		};

		class ACETEAM_COROUTINES_API FErrorNode : public FCoroutineNode
		{
			virtual EStatus Start(FCoroutineExecutor* Exec) override;
		};
	}

	//Convenience node that returns an instant failure
	FCoroutineNodeRef ACETEAM_COROUTINES_API _Error();

	namespace Detail
	{
		class ACETEAM_COROUTINES_API FCoroutineDecorator : public FCoroutineNode
		{
		protected:
			FCoroutineNodePtr m_Child;
		public:
			// Start is normal behavior for decorators, but not forced
			virtual EStatus Start(FCoroutineExecutor* Executor) override;
			void AddChild(FCoroutineNodeRef const& Child)
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
			int m_LastStep;
		
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
			typedef TArray<FCoroutineNodeRef> FChildren;
			FChildren m_Children;
		public:
			void AddChild(FCoroutineNodeRef const& Child);
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
			explicit FTimer(float TargetTime): m_TargetTime (TargetTime) {}
			virtual EStatus Start(FCoroutineExecutor*) override
			{
				m_Timer = m_TargetTime;
				return Running; 
			}
			virtual EStatus Update(FCoroutineExecutor* Exec, float DeltaTime) override
			{
				if (Exec->IsInstant())
					return Completed;
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
			explicit FFrameTimer(int TargetFrames): m_TargetFrames(TargetFrames)
			{
			}

			virtual EStatus Start(FCoroutineExecutor*) override
			{
				m_Frames = m_TargetFrames+1;
				return Running;
			}
			virtual EStatus Update(FCoroutineExecutor* Exec, float) override
			{
				if (Exec->IsInstant())
					return Completed;
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
		inline void AddCoroutineChild(TSharedRef<TCoroutine, DefaultSPMode>& Composite, FCoroutineNodeRef& First)
		{
			Composite->AddChild(First);
		}

		template <typename TCoroutine, typename TLambda, typename TLambdaRetType = void>
		struct TAddCompositeChildHelper
		{
			void operator()(TSharedRef<TCoroutine, DefaultSPMode>& Composite, TLambda& First)
			{
				Composite->AddChild(MakeShared<Detail::TLambdaCoroutine<TLambda>, DefaultSPMode>(First));
			}
		};

		template <typename TCoroutine, typename TLambda>
		struct TAddCompositeChildHelper<TCoroutine, TLambda, bool>
		{
			void operator()(TSharedRef<TCoroutine, DefaultSPMode>& Composite, TLambda& First)
			{
				Composite->AddChild(MakeShared<Detail::TConditionLambdaCoroutine<TLambda>, DefaultSPMode>(First));
			}
		};

		template <typename TCoroutine, typename TLambda>
		struct TAddCompositeChildHelper<TCoroutine, TLambda, FCoroutineNodeRef>
		{
			void operator()(TSharedRef<TCoroutine, DefaultSPMode>& Composite, TLambda& First)
			{
				Composite->AddChild(MakeShared<Detail::TDeferredCoroutineWrapper<TLambda>, DefaultSPMode>(First));
			}
		};

		template <typename TCoroutine, typename TLambda>
		void AddCoroutineChild(TSharedRef<TCoroutine, DefaultSPMode>& Composite, TLambda& First)
		{
			TAddCompositeChildHelper<TCoroutine, TLambda, typename ::TFunctionTraits<decltype(&TLambda::operator())>::RetType>()(Composite, First);
		}

		template <typename TChild>
		void AddCompositeChildren(TSharedRef<FCompositeCoroutine, DefaultSPMode>& Composite, TChild& Child)
		{
			AddCoroutineChild(Composite, Child);
		}
		
		template<typename TChild, typename... TChildren>
		void AddCompositeChildren(TSharedRef<FCompositeCoroutine, DefaultSPMode>& Composite, TChild& First, TChildren... Children)
		{
			AddCoroutineChild(Composite, First);
			AddCompositeChildren(Composite, Children...);
		}
		
		template<typename TComposite, typename... TChildren>
		FCoroutineNodeRef MakeComposite(TChildren... Children)
		{
			auto Comp = StaticCastSharedRef<FCompositeCoroutine>(MakeShared<TComposite, DefaultSPMode>());
			AddCompositeChildren(Comp, Children...);
			return Comp;
		}

		template <typename TScopeLambda>
		struct TScopeHelper
		{
			TScopeLambda m_ScopeLambda;

			TScopeHelper(TScopeLambda& ScopeLambda) : m_ScopeLambda(ScopeLambda) {}

			template <typename TChild>
			FCoroutineNodeRef operator() (TChild&& ScopeBody)
			{
				auto Scope = MakeShared<TScope<TScopeLambda>, DefaultSPMode>(m_ScopeLambda);
				AddCoroutineChild(Scope, ScopeBody);
				return Scope;
			}
		};
	}

	//Runs its arguments in sequence until one returns false
	template<typename ...TChildren>
	FCoroutineNodeRef _Seq(TChildren... Children)
	{
		return Detail::MakeComposite<Detail::FSequence>(Children...);
	}

	//Runs its arguments in parallel until one completes
	template<typename ...TChildren>
	FCoroutineNodeRef _Race(TChildren... Children)
	{
		return Detail::MakeComposite<Detail::FRace>(Children...);
	}

	//Runs its arguments in parallel until all complete
	template<typename ...TChildren>
	FCoroutineNodeRef _Sync(TChildren... Children)
	{
		return Detail::MakeComposite<Detail::FSync>(Children...);
	}

	//Waits for the specified time
	inline FCoroutineNodeRef _Wait(float Time)
	{
		return MakeShared<Detail::FTimer, DefaultSPMode>(Time);
	}

	//Waits for the specified number of frames
	inline FCoroutineNodeRef _WaitFrames(int Frames)
	{
		return MakeShared<Detail::FFrameTimer, DefaultSPMode>(Frames);
	}

	//Loops its child, evaluating at most once per execution step
	template<typename TChild>
	FCoroutineNodeRef _Loop(TChild Body)
	{
		auto Loop = MakeShared<Detail::FLoop, DefaultSPMode>();
		Detail::AddCoroutineChild(Loop, Body);
		return Loop;
	}

	//Shortcut for looping a sequence, similar to a do-while
	template<typename ...TChildren>
	FCoroutineNodeRef _LoopSeq(TChildren... Children)
	{
		auto Seq = _Seq(Children...);
		return _Loop(Seq);
	}

	//Used to execute some code when the execution scope exits, similar to a destructor
	//Usage example:
	//_Scope([] { UE_LOG(LogTemp, Log, TEXT("Child finished or aborted");})
	//(
	//  ... child
	//)
	template<typename TOnScopeExit>
	Detail::TScopeHelper<TOnScopeExit> _Scope(TOnScopeExit&& OnScopeExit)
	{
		return Detail::TScopeHelper<TOnScopeExit>(OnScopeExit);
	}

	//Forks another execution line. The result of executing the child will not affect the original execution.
	template<typename TChild>
	FCoroutineNodeRef _Fork(TChild Body)
	{
		auto Fork = MakeShared<Detail::FFork, DefaultSPMode>();
		Detail::AddCoroutineChild(Fork, Body);
		return Fork;
	}

	namespace Detail
	{
		template <typename TLambda, typename TLambdaRetType = void>
		struct TAddWeakLambdaHelper
		{
			FCoroutineNodeRef operator()(UObject* Obj, TLambda& Lambda)
			{
				return MakeShared<TWeakLambdaCoroutine<TLambda>, DefaultSPMode>(Obj, Lambda);
			}
		};

		template <typename TLambda>
		struct TAddWeakLambdaHelper<TLambda, bool>
		{
			FCoroutineNodeRef operator()(UObject* Obj, TLambda& Lambda)
			{
				return MakeShared<TWeakConditionLambdaCoroutine<TLambda>, DefaultSPMode>(Obj, Lambda);
			}
		};

		template <typename TLambda>
		struct TAddWeakLambdaHelper<TLambda, FCoroutineNodeRef>
		{
			FCoroutineNodeRef operator()(UObject* Obj, TLambda& Lambda)
			{
				return MakeShared<TWeakDeferredCoroutineWrapper<TLambda>, DefaultSPMode>(Obj, Lambda);
			}
		};
	}

	template<typename TLambda>
	FCoroutineNodeRef _Weak(UObject* Obj, TLambda Lambda)
	{
		return Detail::TAddWeakLambdaHelper<TLambda, typename ::TFunctionTraits<decltype(&TLambda::operator())>::RetType>()(Obj, Lambda);
	}
}
