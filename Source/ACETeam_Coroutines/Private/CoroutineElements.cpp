// Copyright ACE Team Software S.A. All Rights Reserved.

#include "CoroutineElements.h"

#include "CoroutineExecutor.h"
#include "CoroutineLog.h"

namespace ACETeam_Coroutines
{
namespace Detail
{
#if WITH_ACETEAM_COROUTINE_DEBUGGER
	int32 FNamedScopeHelper::GetCpuProfilerTraceSpecId() const
	{
		static TMap<FName, int32> CachedIds;
		FName AsName(Name);
		if (auto IntPtr = CachedIds.Find(AsName))
		{
			return *IntPtr;
		}
		int NewId = FCpuProfilerTrace::OutputEventType(*Name);
		CachedIds.Add(AsName, NewId);
		return NewId;
	}
#endif

	void FCompositeCoroutine::AddChild( FCoroutineNodeRef const& Child )
	{
		m_Children.Add(Child);
	}

	void FCompositeCoroutine::End( FCoroutineExecutor* Exec, EStatus Status )
	{
		if (Status == Aborted)
		{
			for (auto& Child : m_Children)
				Exec->AbortNode(Child);
		}
	}

	EStatus FSequence::Start( FCoroutineExecutor* Exec )
	{
		if (m_Children.Num() == 0)
			return Completed;
		m_CurChild = 0;
		Exec->EnqueueCoroutineNode(m_Children[m_CurChild++], this);
		return Suspended;
	}

	EStatus FSequence::OnChildStopped( FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child )
	{
		if (Status == Failed)
		{
			return Failed;
		}
		if (m_CurChild == m_Children.Num())
		{
			return Completed;
		}
		Exec->EnqueueCoroutineNode(m_Children[m_CurChild++], this);
		return Suspended;
	}

	EStatus FOptionalSequence::OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child)
	{
		//don't propagate failure
		if (Status == Failed)
			return Completed;
		return FSequence::OnChildStopped(Exec, Status, Child);
	}

	EStatus FSelect::Start(FCoroutineExecutor* Exec)
	{
		if (m_Children.Num() == 0)
			return Completed;
		m_CurChild = 0;
		Exec->EnqueueCoroutineNode(m_Children[m_CurChild++], this);
		return Suspended;
	}

	EStatus FSelect::OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child)
	{
		if (Status == Completed)
		{
			return Completed;
		}
		if (m_CurChild == m_Children.Num())
		{
			return Status;
		}
		Exec->EnqueueCoroutineNode(m_Children[m_CurChild++], this);
		return Suspended;
	}

	EStatus FParallelBase::Start( FCoroutineExecutor* Exec )
	{
		if (m_Children.Num() == 0)
			return Completed;
		for (int i = m_Children.Num()-1; i >= 0; --i)
		{
			Exec->EnqueueCoroutineNode(m_Children[i], this);
		}
		return Suspended;
	}

	EStatus FRace::OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child)
	{
		AbortOtherBranches(Child, Exec);
		return Status;
	}

	EStatus FSync::Start(FCoroutineExecutor* Exec)
	{
		m_ClosedCount = 0;
		m_EndStatus = Completed;
		return FParallelBase::Start(Exec);
	}

	EStatus FSync::OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child)
	{
		if (Status == Failed)
			m_EndStatus = Failed;
		if (++m_ClosedCount == m_Children.Num())
		{
			return m_EndStatus;
		}
		return Suspended;
	}

	void FParallelBase::AbortOtherBranches( FCoroutineNode* Child, FCoroutineExecutor* Exec )
	{
		for (int i = m_Children.Num()-1; i >= 0; --i)
		{
			if (&m_Children[i].Get() != Child)
			{
				Exec->AbortNode(m_Children[i]);
			}
		}
	}

	void FCoroutineDecorator::End( FCoroutineExecutor* Exec, EStatus Status )
	{
		if (Status == Aborted)
			Exec->AbortNode(m_Child.ToSharedRef());
	}

	EStatus FCoroutineDecorator::OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child)
	{
		return Status;
	}

	EStatus FCoroutineDecorator::Start( FCoroutineExecutor* Exec )
	{
		Exec->EnqueueCoroutineNode(m_Child.ToSharedRef(), this);
		return Suspended;
	}

	void FCoroutineDecorator::AddChild(FCoroutineNodeRef const& Child)
	{
#if DO_CHECK
		if (m_Child)
		{
			UE_LOG(LogACETeamCoroutines, Error, TEXT("Trying to give a decorator more than one child"));
			UE_DEBUG_BREAK();
		}
#endif
		m_Child = Child; 
	}

	EStatus FNot::OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child)
	{
		return Status == Completed ? Failed : Completed;
	}

	FCaptureReturn::FCaptureReturn(TCoroVar<bool> const& InVariable)
		:Variable(InVariable)
	{
	}

	EStatus FCaptureReturn::OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child)
	{
		*Variable = Status == Completed;
		return Completed;
	}
	
	EStatus FCatch::OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child)
	{
		return Completed;
	}

	EStatus FFork::Start( FCoroutineExecutor* Exec )
	{
		Exec->EnqueueCoroutine(m_Child.ToSharedRef());
		return Completed;
	}

	EStatus FLoop::Start( FCoroutineExecutor* Exec )
	{
		m_LastStep = -1;
		return Running;
	}

	EStatus FLoop::Update( FCoroutineExecutor* Exec, float )
	{
		const int CurrentStep = Exec->StepCount();
		if (CurrentStep != m_LastStep)
		{
			m_LastStep = CurrentStep;
			return FCoroutineDecorator::Start(Exec);
		}
		return Running;
	}

	EStatus FLoop::OnChildStopped( FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* )
	{
		if (Status == Failed)
		{
			return Completed;
		}
		return Running;
	}

	EStatus FErrorNode::Start(FCoroutineExecutor* Exec)
	{
		return Failed;
	}

	EStatus FNopNode::Start(FCoroutineExecutor* Exec)
	{
		return Completed;
	}

	EStatus FWaitForeverNode::Start(FCoroutineExecutor* Exec)
	{
		return Suspended;
	}
}

FCoroutineNodeRef _Error()
{
	return MakeShared<Detail::FErrorNode, DefaultSPMode>();
}

FCoroutineNodeRef _Nop()
{
	return MakeShared<Detail::FNopNode, DefaultSPMode>();
}

FCoroutineNodeRef _WaitForever()
{
	return MakeShared<Detail::FWaitForeverNode, DefaultSPMode>();
}

Detail::FCaptureReturnHelper _CaptureReturn(TCoroVar<bool> const& Var)
{
	return Detail::FCaptureReturnHelper(Var);
}

Detail::FNamedScopeHelper _NamedScope(FString const& RootName)
{
	return Detail::FNamedScopeHelper(RootName);
}
}
