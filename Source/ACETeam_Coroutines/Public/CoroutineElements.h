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
					return Completed;
				}
				return Failed;
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
			
		public:
			TWeakDeferredCoroutineWrapper (UObject* Obj, TLambda const& Lambda)
			: TDeferredCoroutineWrapper<TLambda>(Lambda)
			, m_Object(Obj){}
		
			virtual EStatus Start(FCoroutineExecutor* Executor) override
			{
				if (m_Object.IsValid())
				{
					return TDeferredCoroutineWrapper<TLambda>::Start(Executor);
				}
				return Failed;
			}
		};

		class ACETEAM_COROUTINES_API FErrorNode : public FCoroutineNode
		{
			virtual EStatus Start(FCoroutineExecutor* Exec) override;
		};

		class ACETEAM_COROUTINES_API FNopNode : public FCoroutineNode
		{
			virtual EStatus Start(FCoroutineExecutor* Exec) override;
		};

		class ACETEAM_COROUTINES_API FWaitForeverNode : public FCoroutineNode
		{
			virtual EStatus Start(FCoroutineExecutor* Exec) override;
		};
	}

	//Convenience node that returns an instant failure
	FCoroutineNodeRef ACETEAM_COROUTINES_API _Error();

	//Convenience node that does nothing, just completes instantly
	//Useful for returning from deferred coroutine lambdas as a default
	FCoroutineNodeRef ACETEAM_COROUTINES_API _Nop();

	//Convenience node that's suspended forever, useful as a sort of _Nop in a _Race context
	FCoroutineNodeRef ACETEAM_COROUTINES_API _WaitForever();

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
			int m_LastStep = -1;
		
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
				return Status;
			}
			virtual void End(FCoroutineExecutor* Exec, EStatus Status) override
			{
				m_OnScopeEnd();
				return FCoroutineDecorator::End(Exec, Status);
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
	}
	
	typedef TSharedRef<Detail::FCompositeCoroutine, DefaultSPMode> FCoroutineCompositeRef;
	
	namespace Detail {

		class ACETEAM_COROUTINES_API FSequence : public FCompositeCoroutine
		{
			uint32 m_CurChild = 0;
		public:
			virtual EStatus Start(FCoroutineExecutor* Exec) override;
			virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child) override;
		};
		
		class ACETEAM_COROUTINES_API FOptionalSequence : public FSequence
		{
		public:
			virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child) override;
		};

		class ACETEAM_COROUTINES_API FSelect : public FCompositeCoroutine
		{
			uint32 m_CurChild = 0;
		public:
			virtual EStatus Start(FCoroutineExecutor* Exec) override;
			virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child) override;
		};

		class ACETEAM_COROUTINES_API FTimer : public FCoroutineNode
		{
			float m_Timer = 0.0f;
			float m_TargetTime;
		public:
			explicit FTimer(float TargetTime): m_TargetTime (TargetTime) {}
			virtual EStatus Start(FCoroutineExecutor*) override
			{
				if (m_TargetTime <= 0.0f)
					return Completed;
				m_Timer = m_TargetTime;
				return Running; 
			}
			virtual EStatus Update(FCoroutineExecutor* Exec, float DeltaTime) override
			{
				m_Timer -= DeltaTime;
				if (m_Timer <= 0.0f)
					return Completed;
				return Running;
			}
		};

		class ACETEAM_COROUTINES_API FFrameTimer : public FCoroutineNode
		{
			int m_Frames = 0;
			int m_TargetFrames;
		public:
			explicit FFrameTimer(int TargetFrames): m_TargetFrames(TargetFrames)
			{
			}

			virtual EStatus Start(FCoroutineExecutor*) override
			{
				if (m_TargetFrames <= 0)
					return Completed;
				m_Frames = m_TargetFrames+1;
				return Running;
			}
			virtual EStatus Update(FCoroutineExecutor* Exec, float) override
			{
				--m_Frames;
				if (m_Frames <= 0)
					return Completed;
				return Running;
			}
		};

		template <typename F>
		class TDynamicTimer : public FCoroutineNode
		{
			F m_Lambda;
			float m_Timer = 0.0f;
		public:
			explicit TDynamicTimer(F const& Lambda) : m_Lambda(Lambda)
			{}
			virtual EStatus Start(FCoroutineExecutor* Exec) override
			{
				m_Timer = static_cast<float>(m_Lambda());
				return m_Timer > 0.0f ? Running : Completed;
			}
			virtual EStatus Update(FCoroutineExecutor* Exec, float dt) override
			{
				m_Timer -= dt;
				if (m_Timer < 0.0f)
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

		template<typename TCoroutine, typename TChildNode>
		typename TEnableIf<TIsDerivedFrom<TChildNode, FCoroutineNode>::Value, void>::Type
		AddCoroutineChild(TSharedRef<TCoroutine, DefaultSPMode>& Composite, TSharedRef<TChildNode, DefaultSPMode> const& First)
		{
			Composite->AddChild(First);
		}

		template <typename TLambdaRetType = void, typename Enable=void>
		struct TNodeTemplateForLambdaRetType
		{
			//template<typename TLambda>
			//using Value = TSomeTemplate<TLambda>;
		};

		template <>
		struct TNodeTemplateForLambdaRetType<void>
		{
			template<typename TLambda>
			using Value = TLambdaCoroutine<TLambda>;
		};

		template <>
		struct TNodeTemplateForLambdaRetType<bool>
		{
			template<typename TLambda>
			using Value = TConditionLambdaCoroutine<TLambda>;
		};

		template <typename TCoroutine>
		struct TNodeTemplateForLambdaRetType<TSharedRef<TCoroutine, DefaultSPMode>,
		typename TEnableIf<TIsDerivedFrom<TCoroutine, FCoroutineNode>::Value, void>::Type>
		{
			template<typename TLambda>
			using Value = TDeferredCoroutineWrapper<TLambda>;
		};

		template <typename TCoroutine, typename TLambda>
		typename TEnableIf<TIsFunctor<TLambda>::value, void>::Type
		AddCoroutineChild(TSharedRef<TCoroutine, DefaultSPMode>& Composite, TLambda& First)
		{
			typedef typename ::TFunctorTraits<TLambda>::RetType RetType;
			Composite->AddChild(MakeShared<typename TNodeTemplateForLambdaRetType<RetType>::template Value<TLambda>, DefaultSPMode>(First));
		}
		
		inline void AddCompositeChildren(TSharedRef<FCompositeCoroutine, DefaultSPMode>&)
		{
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
		FCoroutineCompositeRef MakeComposite(TChildren... Children)
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

	//Runs its arguments in sequence until one returns false, propagates failure upwards
	template<typename ...TChildren>
	FCoroutineCompositeRef _Seq(TChildren... Children)
	{
		return Detail::MakeComposite<Detail::FSequence>(Children...);
	}

	//Runs its arguments in sequence until one returns false, catches failures
	template<typename ...TChildren>
	FCoroutineCompositeRef _OptionalSeq(TChildren... Children)
	{
		return Detail::MakeComposite<Detail::FOptionalSequence>(Children...);
	}

	//Runs its arguments in sequence until one finishes successfully, propagates failure if last arg fails
	template<typename ...TChildren>
	FCoroutineCompositeRef _Select(TChildren... Children)
	{
		return Detail::MakeComposite<Detail::FSelect>(Children...);
	}

	//Runs its arguments in parallel until one completes, propagates failure if the child that finished the race failed
	template<typename ...TChildren>
	FCoroutineCompositeRef _Race(TChildren... Children)
	{
		return Detail::MakeComposite<Detail::FRace>(Children...);
	}

	//Runs its arguments in parallel until all complete, propagates failure if any child failed
	template<typename ...TChildren>
	FCoroutineCompositeRef _Sync(TChildren... Children)
	{
		return Detail::MakeComposite<Detail::FSync>(Children...);
	}

	namespace Detail
	{
		template <typename T, bool bConvertibleToFloat>
		struct TimerArgHelper
		{
			static FCoroutineNodeRef Make(T const& Arg)
			{
				typedef T TLambda;
				static_assert(TIsArithmetic<typename ::TFunctorTraits<TLambda>::RetType>::Value, "Return type of lambda must be convertible to float");
				return MakeShared<Detail::TDynamicTimer<TLambda>, DefaultSPMode>(Arg);
			}
		};

		template <typename T>
		struct TimerArgHelper<T, true>
		{
			static FCoroutineNodeRef Make(float Arg)
			{
				return MakeShared<Detail::FTimer, DefaultSPMode>(Arg);
			}
		};
	}

	//Waits for the specified number of seconds. Can also receive a lambda that returns the number of seconds when evaluated
	template <typename T>
	FCoroutineNodeRef _Wait(T const& Arg)
	{
		return Detail::TimerArgHelper<T, TIsArithmetic<T>::Value>::Make(Arg);
	}

	//Waits for the specified number of seconds fetched from shared float
	inline FCoroutineNodeRef _Wait(TSharedRef<float> const& FloatVar)
	{
		return _Wait([=]{ return *FloatVar; });
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

	//Forks another execution line. Will not wait for the contained elements. The result of executing the child will not affect the original execution.
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

	//The body will not be executed if the associated UObject no longer exists
	template<typename TLambda>
	FCoroutineNodeRef _Weak(UObject* Obj, TLambda Lambda)
	{
		return Detail::TAddWeakLambdaHelper<TLambda, typename ::TFunctorTraits<TLambda>::RetType>()(Obj, Lambda);
	}
}
