// Copyright ACE Team Software S.A. All Rights Reserved.
#include "CoroutineElements.h"

#include "CoroutineExecutor.h"

namespace ACETeam_Coroutines
{
namespace detail
{
	void FCompositeCoroutine::AddChild( FCoroutinePtr const& Child )
	{
		m_Children.Add(Child);
	}

	void FCompositeCoroutine::End( FCoroutineExecutor* Executor, EStatus Status )
	{
		if (Status == Aborted)
		{
			for (auto& child : m_Children)
				Executor->AbortTask(child.Get());
		}
	}

	EStatus FSequence::Start( FCoroutineExecutor* Executor )
	{
		if (m_Children.Num() == 0)
			return Completed;
		m_CurChild = 0;
		Executor->OpenCoroutine(m_Children[m_CurChild++], this);
		return Suspended;
	}

	EStatus FSequence::OnChildStopped( FCoroutineExecutor* Executor, EStatus Status, FCoroutine* Child )
	{
		if (Status == Failed)
		{
			return Failed;
		}
		if (m_CurChild == m_Children.Num())
		{
			return Completed;
		}
		Executor->OpenCoroutine(m_Children[m_CurChild++], this);
		return Suspended;
	}

	EStatus FParallelBase::Start( FCoroutineExecutor* Exec )
	{
		for (int i = m_Children.Num()-1; i >= 0; --i)
		{
			Exec->OpenCoroutine(m_Children[i], this);
		}
		return Suspended;
	}

	EStatus FRace::OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutine* Child)
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

	EStatus FSync::OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutine* Child)
	{
		if (Status == Failed)
			m_EndStatus = Failed;
		if (m_ClosedCount == m_Children.Num())
		{
			return m_EndStatus;
		}
		return Suspended;
	}

	void FParallelBase::AbortOtherBranches( FCoroutine* Child, FCoroutineExecutor* Exec )
	{
		for (int i = m_Children.Num()-1; i >= 0; --i)
		{
			if (m_Children[i].Get() != Child)
			{
				Exec->AbortTask(m_Children[i].Get());
			}
		}
	}

	void FCoroutineDecorator::End( FCoroutineExecutor* Exec, EStatus Status )
	{
		if (Status == Aborted)
			Exec->AbortTask(m_Child);
	}

	EStatus FCoroutineDecorator::Start( FCoroutineExecutor* Exec )
	{
		Exec->OpenCoroutine(m_Child, this);
		return Suspended;
	}

	EStatus FBranch::Start( FCoroutineExecutor* Exec )
	{
		Exec->OpenCoroutine(m_Child);
		return Completed;
	}

	EStatus FLoop::Start( FCoroutineExecutor* Exec )
	{
		m_LastOpen = -1;
		return Running;
	}

	EStatus FLoop::Update( FCoroutineExecutor* Exec, float )
	{
		int currentStep = GFrameCounter;
		if (currentStep != m_LastOpen)
		{
			m_LastOpen = currentStep;
			return FCoroutineDecorator::Start(Exec);
		}
		return Running;
	}

	EStatus FLoop::OnChildStopped( FCoroutineExecutor* Exec, EStatus Status, FCoroutine* )
	{
		if (Status == Failed)
		{
			return Completed;
		}
		return Running;
	}
}
}