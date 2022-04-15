// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "Coroutine.h"
#include "Containers/RingBuffer.h"

namespace ACETeam_Coroutines
{
	class ACETEAM_COROUTINES_API FCoroutineExecutor
	{
		enum
		{
			None = 1<<6,
			Active = ~(Completed|Failed),
		};

		struct FCoroutineInfo
		{
			FCoroutineInfo()
			{
				Parent = nullptr;
				Status = (EStatus)None;
			}
			FCoroutinePtr Coroutine;
			FCoroutine* Parent;
			EStatus Status;
		};
		
		typedef TRingBuffer<FCoroutineInfo> ActiveTasks;
		ActiveTasks m_ActiveCoroutines;

		typedef TArray<FCoroutineInfo> SuspendedTasks;
		SuspendedTasks m_SuspendedTasks;

		bool SingleStep(float DeltaTime);
		
		void ProcessTaskEnd(FCoroutineInfo& Info, EStatus Status);

		void Cleanup();

		bool IsActive(const FCoroutineInfo& t) const
		{
			return (t.Status & Active) != 0;
		}
		struct Coroutine_Is
		{
			FCoroutine* m_Coroutine;
			Coroutine_Is(FCoroutine* Coroutine) : m_Coroutine(Coroutine) {}
			bool operator() (const FCoroutineInfo& t) { return t.Coroutine.Get() == m_Coroutine; }
		};
	public:
		FCoroutineExecutor();

		void OpenCoroutine(FCoroutinePtr const& Coroutine, FCoroutine* Parent = nullptr);

		// This function silently drops a task from the scheduler,
		// only telling the task itself about it.
		// Normally used by parallels to abort branches.
		// It does not alert the parent, because either it is the parallel and already knows about it,
		// or it's an intermediate composite node that doesn't need to know because it will be aborted as well.
		void AbortTask(FCoroutine* Coroutine);

		void AbortTask(FCoroutinePtr const& Coroutine) { AbortTask(Coroutine.Get()); }

		// Finds the root of the tree containing this task, and aborts the whole tree
		// Use of this function should be limited to the handling of fatal errors
		void AbortTree(FCoroutine* Coroutine);
		
		void AbortTree(FCoroutinePtr const& Coroutine) { AbortTree(Coroutine.Get()); }

		// This function can be used to force a task to end outside of the normal functioning of the scheduler.
		// For instance, a task whose only purpose is to wait suspended for something to happen can be notified in this
		// way. Note however that any dependent tasks will not be updated until the scheduler's next step.
		void ForceTaskEnd(FCoroutine* Coroutine, EStatus Status);

		void ForceTaskEnd(FCoroutinePtr Coroutine, EStatus Status) { ForceTaskEnd(Coroutine.Get(), Status); }

		void Step(float DeltaTime)
		{
			while (SingleStep(DeltaTime)) { continue; }
			Cleanup();
		}

		enum TaskFindResult
		{
			TFR_NotRunning,
			TFR_Suspended,
			TFR_Running,
		};
		TaskFindResult FindTask(FCoroutinePtr Coroutine);
	};

	namespace detail
	{
		template <typename TLambda>
		class TDeferredCoroutineWrapper : public FCoroutine
		{
			TLambda m_Lambda;
			FCoroutinePtr m_Child;
		public:
			TDeferredCoroutineWrapper (TLambda const& Lambda) : m_Lambda(Lambda) {}
			virtual EStatus Start(FCoroutineExecutor* Executor) override
			{
				m_Child = m_Lambda();
				Executor->OpenCoroutine(m_Child,this);
				return Suspended; 
			}
			virtual EStatus OnChildStopped(FCoroutineExecutor*, EStatus Status, FCoroutine*) override { return Status; }
			virtual void End(FCoroutineExecutor* Executor, EStatus Status) override
			{
				if (Status == Aborted)
				{
					Executor->AbortTask(m_Child);
				}
			};
		};
	}

	//Make a simple coroutine out of a function, functor or lambda that returns a coroutine pointer
	template <typename TLambda>
	FCoroutinePtr _Deferred(TLambda& Lambda) { return MakeShared<detail::TDeferredCoroutineWrapper<TLambda> >(Lambda); }
	
}