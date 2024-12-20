// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "CoroutineExecutor.h"
#include "CoroutineParameter.h"

namespace ACETeam_Coroutines
{
	class ACETEAM_COROUTINES_API FCoroutineExecutor; //forward declaration so friend class declarations work correctly
	
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
#if WITH_ACETEAM_COROUTINE_DEBUGGER
			virtual bool Debug_IsDeferredNodeGenerator() const override { return true; }
#endif
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
#if WITH_ACETEAM_COROUTINE_DEBUGGER
			virtual FString Debug_GetName() const override { return TEXT("Wait forever"); }
#endif
		};
	}

	//Convenience node that returns an instant failure
	FCoroutineNodeRef ACETEAM_COROUTINES_API _Error();

	//Convenience node that does nothing, just completes instantly
	//Useful for returning from deferred coroutine lambdas as a default
	FCoroutineNodeRef ACETEAM_COROUTINES_API _Nop();

	//Convenience node that's suspended forever, useful as a sort of _Nop in a _Race context
	FCoroutineNodeRef ACETEAM_COROUTINES_API _WaitForever();

	//Convert a lambda into a coroutine node according to its return type
	//This is usually done automatically in the context of existing nodes, but you can use this to force the conversion
	//when you don't have another node to add this to.
	template <typename TLambda>
	typename TEnableIf<TIsFunctor<TLambda>::value, FCoroutineNodeRef>::Type
		_ConvertLambda(TLambda const& Lambda)
	{
		typedef typename ::TFunctorTraits<TLambda>::RetType RetType;
		return MakeShared<typename Detail::TNodeTemplateForLambdaRetType<RetType>::template Value<TLambda>, DefaultSPMode>(Lambda);
	}

	namespace Detail
	{
		class ACETEAM_COROUTINES_API FCoroutineDecorator : public FCoroutineNode
		{
		protected:
			FCoroutineNodePtr m_Child;
		public:
			// Default behavior for decorator is to run child and suspend
			virtual EStatus Start(FCoroutineExecutor* Executor) override;
			void AddChild(FCoroutineNodeRef const& Child);
			virtual void End(FCoroutineExecutor* Executor, EStatus Status) override;
			virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child) override;
		};

		class ACETEAM_COROUTINES_API FNot : public FCoroutineDecorator
		{
			virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child) override;
		};

		class ACETEAM_COROUTINES_API FCaptureReturn : public FCoroutineDecorator
		{
			TCoroVar<bool> Variable;
		public:
			FCaptureReturn(TCoroVar<bool> const& InVariable);
			virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child) override;
		};

		struct FCaptureReturnHelper
		{
			TCoroVar<bool> Variable;

			FCaptureReturnHelper(TCoroVar<bool> const& InVariable) : Variable(InVariable) {}

			template<typename TChild>
			FCoroutineNodeRef operator() (TChild&& Body)
			{
				auto CaptureReturn = MakeShared<FCaptureReturn, DefaultSPMode>(Variable);
				AddCoroutineChild(CaptureReturn, Body);
				return CaptureReturn;
			}
		};
		
		class ACETEAM_COROUTINES_API FCatch : public FCoroutineDecorator
		{
			virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child) override;
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

#if WITH_ACETEAM_COROUTINE_DEBUGGER
			virtual FString Debug_GetName() const override { return TEXT("Loop"); }
#endif
		};

		template<typename TScopeEndedLambda>
		class TScope : public FCoroutineDecorator
		{
			TScopeEndedLambda m_OnScopeEnd;
		public:
			TScope(TScopeEndedLambda const& OnScopeEnd) : m_OnScopeEnd(OnScopeEnd){}
			virtual void End(FCoroutineExecutor* Exec, EStatus Status) override
			{
				CallLambda(Status);
				return FCoroutineDecorator::End(Exec, Status);
			}
		protected:
			void CallLambda(EStatus Status)
			{
				static_assert(std::is_void_v<typename TFunctorTraits<TScopeEndedLambda>::RetType>,
					"Scope lambdas must have void return type");
				if constexpr (TFunctorTraits<TScopeEndedLambda>::ArgCount > 0)
				{
					static_assert(TFunctorTraits<TScopeEndedLambda>::ArgCount == 1 &&
						std::is_same_v<typename TFunctorTraits<TScopeEndedLambda>::template NthArg<0>, EStatus>,
						"Scope lambdas can only receive one EStatus argument or no arguments");
					m_OnScopeEnd(Status);
				}
				else
				{
					m_OnScopeEnd();
				}
			}
		};

		template<typename TScopeEndedLambda>
		class TScopeWeak : public TScope<TScopeEndedLambda>
		{
			FWeakObjectPtr m_Owner;
		public:
			TScopeWeak(UObject* Owner, TScopeEndedLambda const& Lambda)
				: TScope<TScopeEndedLambda>(Lambda)
				, m_Owner(Owner)
			{}
			virtual void End(FCoroutineExecutor* Exec, EStatus Status) override
			{
				if (m_Owner.IsValid())
					this->CallLambda(Status);
				return FCoroutineDecorator::End(Exec, Status);
			}
		};
		
		struct FScopeHelper
		{
			TSharedRef<FCoroutineDecorator, DefaultSPMode> m_Scope;

			FScopeHelper(TSharedRef<FCoroutineDecorator, DefaultSPMode> const& Scope) : m_Scope(Scope) {}

			template <typename TChild>
			FCoroutineNodeRef operator() (TChild&& Body)
			{
				AddCoroutineChild(m_Scope, Body);
				return m_Scope;
			}
		};

		class ACETEAM_COROUTINES_API FNamedScopeNode : public FCoroutineDecorator
		{
		protected:
#if WITH_ACETEAM_COROUTINE_DEBUGGER
			FNamedScopeNode* ParentScope = nullptr; //set by executor
			FString Name;
			int32 CpuTraceSpecId;
			virtual FString Debug_GetName() const override { return Name; }
			virtual bool Debug_IsDebuggerScope() const override { return true; }
			friend class ::ACETeam_Coroutines::FCoroutineExecutor;
#endif
		public:
			FNamedScopeNode(FString&& InName, int InCpuTraceId)
#if WITH_ACETEAM_COROUTINE_DEBUGGER
				: Name(InName)
				, CpuTraceSpecId(InCpuTraceId)
#endif
			{}
			
		};

		struct ACETEAM_COROUTINES_API FNamedScopeHelper
		{
			FString Name;

			FNamedScopeHelper(FString const& InName) : Name(InName) {}

#if WITH_ACETEAM_COROUTINE_DEBUGGER
			int32 GetCpuProfilerTraceSpecId() const;
#endif

			template <typename TChild>
			FCoroutineNodeRef operator[] (TChild&& Body)
			{
#if WITH_ACETEAM_COROUTINE_DEBUGGER
				auto NamedRoot = MakeShared<FNamedScopeNode, DefaultSPMode> (MoveTemp(Name), GetCpuProfilerTraceSpecId());
				AddCoroutineChild(NamedRoot, Body);
				return NamedRoot;
#else
				return Body;
#endif
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
			
#if WITH_ACETEAM_COROUTINE_DEBUGGER
			virtual FString Debug_GetName() const override { return TEXT("Seq"); }
#endif
		};
		
		class ACETEAM_COROUTINES_API FOptionalSequence : public FSequence
		{
		public:
			virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child) override;

#if WITH_ACETEAM_COROUTINE_DEBUGGER
			virtual FString Debug_GetName() const override { return TEXT("OptionalSeq"); }
#endif
		};

		class ACETEAM_COROUTINES_API FSelect : public FCompositeCoroutine
		{
			uint32 m_CurChild = 0;
		public:
			virtual EStatus Start(FCoroutineExecutor* Exec) override;
			virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child) override;

#if WITH_ACETEAM_COROUTINE_DEBUGGER
			virtual FString Debug_GetName() const override { return TEXT("Select"); }
#endif
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

#if WITH_ACETEAM_COROUTINE_DEBUGGER
			virtual FString Debug_GetName() const override { return FString::Printf(TEXT("Wait %.1fs"), m_TargetTime); }
#endif
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

#if WITH_ACETEAM_COROUTINE_DEBUGGER
			virtual FString Debug_GetName() const override { return FString::Printf(TEXT("Wait %d frames"), m_TargetFrames); }
#endif
		};

		template <typename F>
		class TDynamicTimer : public FCoroutineNode
		{
			F m_Lambda;
			float m_Timer = 0.0f;
#if WITH_ACETEAM_COROUTINE_DEBUGGER
			float m_DebugLastTimer = 0.0f;
#endif
		public:
			explicit TDynamicTimer(F const& Lambda) : m_Lambda(Lambda)
			{}
			virtual EStatus Start(FCoroutineExecutor* Exec) override
			{
				float CurrentTimer = static_cast<float>(m_Lambda());
#if WITH_ACETEAM_COROUTINE_DEBUGGER
				m_DebugLastTimer = CurrentTimer;
#endif
				m_Timer = CurrentTimer;
				return m_Timer > 0.0f ? Running : Completed;
			}
			virtual EStatus Update(FCoroutineExecutor* Exec, float dt) override
			{
				m_Timer -= dt;
				if (m_Timer < 0.0f)
					return Completed;
				return Running;
			}

#if WITH_ACETEAM_COROUTINE_DEBUGGER
			virtual FString Debug_GetName() const override { return FString::Printf(TEXT("Wait %.1fs"), m_DebugLastTimer); }
#endif
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
#if WITH_ACETEAM_COROUTINE_DEBUGGER
			virtual FString Debug_GetName() const override { return TEXT("Race"); }
#endif
		};

		class ACETEAM_COROUTINES_API FSync : public FParallelBase
		{
			uint32 m_ClosedCount = 0;
			EStatus m_EndStatus = Completed;
		public:
			virtual EStatus Start(FCoroutineExecutor* Exec) override;
			virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child) override;
#if WITH_ACETEAM_COROUTINE_DEBUGGER
			virtual FString Debug_GetName() const override { return TEXT("Sync"); }
#endif
		};

		template<typename TCoroutine, typename TChildNode>
		typename TEnableIf<TIsDerivedFrom<TChildNode, FCoroutineNode>::Value, void>::Type
		AddCoroutineChild(TSharedRef<TCoroutine, DefaultSPMode>& Composite, TSharedRef<TChildNode, DefaultSPMode> const& First)
		{
			Composite->AddChild(First);
		}

		template <typename TCoroutine, typename TLambda>
		typename TEnableIf<TIsFunctor<TLambda>::value, void>::Type
		AddCoroutineChild(TSharedRef<TCoroutine, DefaultSPMode>& Composite, TLambda& First)
		{
			Composite->AddChild(_ConvertLambda(First));
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
	}

	//Runs its arguments in sequence until one returns false, propagates failure upwards
	template<typename ...TChildren>
	FCoroutineCompositeRef _Seq(TChildren... Children)
	{
		return Detail::MakeComposite<Detail::FSequence>(Children...);
	}

	//Runs its arguments in sequence until one returns false, catches failures (i.e. never fails)
	template<typename ...TChildren>
	FCoroutineCompositeRef _OptionalSeq(TChildren... Children)
	{
		return Detail::MakeComposite<Detail::FOptionalSequence>(Children...);
	}

	//Alias for _OptionalSeq
	template<typename ...TChildren>
	FCoroutineCompositeRef _SeqNoFail(TChildren... Children)
	{
		return _OptionalSeq(Children...);
	}

	//Runs its arguments in sequence, moving on when one fails, until one finishes successfully, propagates failure if last child fails
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
				return MakeShared<TDynamicTimer<TLambda>, DefaultSPMode>(Arg);
			}
		};

		template <typename T>
		struct TimerArgHelper<T, true>
		{
			static FCoroutineNodeRef Make(float Arg)
			{
				return MakeShared<FTimer, DefaultSPMode>(Arg);
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
	inline FCoroutineNodeRef _Wait(TCoroVar<float> const& FloatVar)
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
	//Lambdas used by _Scope must have void return type. They can optionally receive an EStatus argument
	//that receives the termination status of their contained child.
	//Usage example:
	//_Scope([] { UE_LOG(LogTemp, Log, TEXT("Child finished or aborted");})
	//(
	//  ... child
	//)
	template<typename TOnScopeExit>
	Detail::FScopeHelper _Scope(TOnScopeExit const& OnScopeExit)
	{
		return Detail::FScopeHelper(MakeShared<Detail::TScope<TOnScopeExit>, DefaultSPMode>(OnScopeExit));
	}

	//A scope that only evaluates its exit lambda if its owning object is still valid
	//This should only be used for cleanup specific to that object, otherwise you might skip important cleanup logic!
	template<typename TOnScopeExit>
	Detail::FScopeHelper _ScopeWeak(UObject* Owner, TOnScopeExit const& OnScopeExit)
	{
		return Detail::FScopeHelper(MakeShared<Detail::TScopeWeak<TOnScopeExit>, DefaultSPMode>(Owner, OnScopeExit));
	}

	//Negates the success or failure of its child.
	template<typename TChild>
	FCoroutineNodeRef _Not(TChild Body)
	{
		auto Not = MakeShared<Detail::FNot, DefaultSPMode>();
		Detail::AddCoroutineChild(Not, Body);
		return Not;
	}

	//Captures the return value of its child into a shared bool variable, prevents failure from propagating upward
	Detail::FCaptureReturnHelper ACETEAM_COROUTINES_API _CaptureReturn(TCoroVar<bool> const& Var);

	//Catches any failure and prevents it from propagating upward. Similar to _CaptureReturn, but ignores the return value
	template<typename TChild>
	FCoroutineNodeRef _Catch(TChild Body)
	{
		auto Catch = MakeShared<Detail::FCatch, DefaultSPMode>();
		Detail::AddCoroutineChild(Catch, Body);
		return Catch;
	}

	//Forks another execution line. Will not wait for the contained elements. The result of executing the child will not affect the original execution.
	template<typename TChild>
	FCoroutineNodeRef _Fork(TChild Body)
	{
		auto Fork = MakeShared<Detail::FFork, DefaultSPMode>();
		Detail::AddCoroutineChild(Fork, Body);
		return Fork;
	}

	/**
	 * Used as a way to give a name to a coroutine that will show up in the debugger, so it won't just be an anonymous block running
	 */
	Detail::FNamedScopeHelper ACETEAM_COROUTINES_API _NamedScope(FString const& RootName);
	
	namespace Detail
	{
		template <typename TLambda, typename TLambdaRetType = void, typename Enable=void>
		struct TAddWeakLambdaHelper
		{
		};

		template <typename TLambda>
		struct TAddWeakLambdaHelper<TLambda, void>
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

		template <typename TLambda, typename TCoroutine>
		struct TAddWeakLambdaHelper<TLambda, TSharedRef<TCoroutine, DefaultSPMode>,
			typename TEnableIf<TIsDerivedFrom<TCoroutine, FCoroutineNode>::Value, void>::Type>
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
