// Copyright ACE Team Software S.A. All Rights Reserved.
#include "CoroutineSemaphores.h"

#include "CoroutineLog.h"

namespace ACETeam_Coroutines
{
	namespace Detail
	{
		EStatus FSemaphoreHandlerNode::Start(FCoroutineExecutor* Exec)
		{
			if (Semaphore->TryTake(AsShared()))
			{
				return FCoroutineDecorator::Start(Exec);
			}
			CachedExec = Exec;
			return Suspended;
		}

		EStatus FSemaphoreHandlerNode::OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child)
		{
			Semaphore->Release();
			return Status;
		}

		void FSemaphoreHandlerNode::End(FCoroutineExecutor* Exec, EStatus Status)
		{
			FCoroutineDecorator::End(Exec, Status);
			if (CachedExec == nullptr && Status == Aborted)
			{
				Semaphore->Release();
			}
			else if (CachedExec)
			{
				ensure(Semaphore->DropFromQueue(AsShared()));
			}
			CachedExec = nullptr;
		}

		void FSemaphoreHandlerNode::Resume()
		{
			check(CachedExec);
			CachedExec->EnqueueCoroutineNode(m_Child.ToSharedRef(), this);
			CachedExec = nullptr;
		}

		bool FSemaphore::TryTake(FSemaphoreHandlerRef const& Handler)
		{
			if (IsAvailable())
			{
				++CurrentActive;
				return true;
			}
			QueuedHandlers.Add(Handler);
			return false;
		}

		bool FSemaphore::DropFromQueue(FSemaphoreHandlerRef const& Handler)
		{
			return QueuedHandlers.Remove(Handler) > 0;
		}

		void FSemaphore::Release()
		{
			check(CurrentActive > 0);
			if (QueuedHandlers.Num())
			{
				const FSemaphoreHandlerRef Current = QueuedHandlers[0];
				QueuedHandlers.RemoveAt(0);
				Current->Resume();
				return;
			}
			--CurrentActive;
		}

		void FSemaphore::SetMaxActive(int NewMaxActive)
		{
			check(NewMaxActive > 0);
			if (NewMaxActive < CurrentActive)
			{
				UE_LOG(LogACETeamCoroutines, Warning, TEXT("Setting max active to a lower value than the currently running coroutines. (NewMax: %d, Current: %d)"), NewMaxActive, CurrentActive);
			}
			const int NumToStartNow = FMath::Min(QueuedHandlers.Num(), NewMaxActive-MaxActive);
			if (CurrentActive == MaxActive && NumToStartNow > 0)
			{
				//Split queued handles in two. One part will be started now, the other part will remain queued
				decltype(QueuedHandlers) ToStart;
				decltype(QueuedHandlers) NewQueued;
				ToStart.Reserve(NumToStartNow);
				for (int i = 0; i < NumToStartNow; ++i)
				{
					ToStart.Emplace(MoveTemp(QueuedHandlers[i]));
				}
				NewQueued.Reserve(QueuedHandlers.Num()-NumToStartNow);
				for (int i = NumToStartNow; i < QueuedHandlers.Num(); ++i)
				{
					NewQueued.Emplace(MoveTemp(QueuedHandlers[i]));
				}
				QueuedHandlers = MoveTemp(NewQueued);
				for (auto const& StartingHandler : ToStart)
				{
					StartingHandler->Resume();
				}
			}
			MaxActive = NewMaxActive;
		}
	}

	Detail::FSemaphoreHelper _SemaphoreScope(FSemaphoreRef const& Semaphore)
	{
		return Detail::FSemaphoreHelper(Semaphore);
	}

	FSemaphoreRef MakeSemaphore(int MaxActive)
	{
		return MakeShared<Detail::FSemaphore>(MaxActive);
	}
}
