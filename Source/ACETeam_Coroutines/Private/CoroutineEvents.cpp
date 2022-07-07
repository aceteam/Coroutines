// Copyright ACE Team Software S.A. All Rights Reserved.
#include "CoroutineEvents.h"

namespace ACETeam_Coroutines
{
	namespace Detail
	{
		void FEventBase::AddListener(FEventListenerRef const& Listener)
		{
			check(IsInGameThread());
			check(!Listeners.Contains(Listener));
			Listeners.Add(Listener);
		}

		void FEventBase::RemoveListener(FEventListenerRef const& Listener)
		{
			check(IsInGameThread());
			Listeners.RemoveSingleSwap(Listener);
		}

		void FEventBase::AbortListeners()
		{
			check(IsInGameThread());
			auto TempListeners = MoveTemp(Listeners);
			for (const auto& Listener : TempListeners)
			{
				Listener->EventAborted();
			}
		}

		EStatus FEventListenerBase::Start(FCoroutineExecutor* Exec)
		{
			Event->AddListener(this->AsShared());
			CachedExec = Exec;
			return Suspended;
		}

		void FEventListenerBase::End(FCoroutineExecutor* Exec, EStatus Status)
		{
			if (Status == Aborted)
			{
				Event->RemoveListener(this->AsShared());
			}
			CachedExec = nullptr;
		}

		void FEventListenerBase::ReceiveEvent()
		{
			if (CachedExec)
			{
				CachedExec->ForceNodeEnd(this, Completed);
			}
		}

		void FEventListenerBase::EventAborted()
		{
			if (CachedExec)
			{
				CachedExec->ForceNodeEnd(this, Failed);
			}
		}
	}

	FCoroutineNodeRef _WaitFor(TEventRef<void> const& Event)
	{
		return MakeShared<Detail::FEventListenerBase, DefaultSPMode>(Event);
	}
}
