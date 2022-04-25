// Copyright ACE Team Software S.A. All Rights Reserved.
#include "CoroutineElements.h"

#include "CoroutineExecutor.h"

namespace ACETeam_Coroutines
{
namespace Detail
{
	void FCompositeCoroutine::AddChild( FCoroutineNodePtr const& Child )
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

	EStatus FParallelBase::Start( FCoroutineExecutor* Exec )
	{
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
			if (m_Children[i].Get() != Child)
			{
				Exec->AbortNode(m_Children[i].Get());
			}
		}
	}

	void FCoroutineDecorator::End( FCoroutineExecutor* Exec, EStatus Status )
	{
		if (Status == Aborted)
			Exec->AbortNode(m_Child);
	}

	EStatus FCoroutineDecorator::Start( FCoroutineExecutor* Exec )
	{
		Exec->EnqueueCoroutineNode(m_Child, this);
		return Suspended;
	}

	EStatus FFork::Start( FCoroutineExecutor* Exec )
	{
		Exec->EnqueueCoroutine(m_Child);
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
}

FCoroutineNodePtr _Error()
{
	return MakeShared<Detail::FErrorNode>();
}
}
