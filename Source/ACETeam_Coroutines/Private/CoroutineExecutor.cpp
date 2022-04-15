#include "CoroutineExecutor.h"
#include "Algo/Find.h"

bool ACETeam_Coroutines::FCoroutineExecutor::SingleStep( float DeltaTime )
{
	FCoroutineInfo Info = m_ActiveCoroutines.Last();
	m_ActiveCoroutines.Pop();

	if (Info.Coroutine.Get() == nullptr)
	{
		//we've reached the marker, it's time to quit for this step
		m_ActiveCoroutines.AddFront(Info);
		return false;
	}
	
	if (Info.Status == Aborted)
	{
		return true;
	}

	//task is just starting, lets eval its starting condition
	if (Info.Status == None)
	{
		Info.Status = Info.Coroutine->Start(this);

		//suspended tasks get thrown into the suspended list
		if (Info.Status == Suspended)
		{
			m_SuspendedTasks.Add(Info);
			return true;
		}
		//check if it's already done
		if (!IsActive(Info))
		{
			ProcessTaskEnd(Info, Info.Status);
			return true;
		}
	}

	Info.Status = Info.Coroutine->Update(this, DeltaTime);

	//suspended tasks get thrown into the suspended list
	if (Info.Status == Suspended)
	{
		m_SuspendedTasks.Add(Info);
		return true;
	}
	if (!IsActive(Info))
	{
		ProcessTaskEnd(Info, Info.Status);
		return true;
	}
	
	m_ActiveCoroutines.AddFront(Info);
	return true;
}


ACETeam_Coroutines::FCoroutineExecutor::FCoroutineExecutor()
{
	m_ActiveCoroutines.Add(FCoroutineInfo()); //add frame marker
}

void ACETeam_Coroutines::FCoroutineExecutor::OpenCoroutine(FCoroutinePtr const& Coroutine, FCoroutine* Parent)
{
	FCoroutineInfo CoroutineInfo;
	CoroutineInfo.Coroutine = Coroutine;
	CoroutineInfo.Parent = Parent;
	CoroutineInfo.Status = (EStatus)None;
	m_ActiveCoroutines.Add(CoroutineInfo);
}

void ACETeam_Coroutines::FCoroutineExecutor::ProcessTaskEnd( FCoroutineInfo& Info, EStatus Status )
{
	Info.Coroutine->End(this, Status);
	if (Info.Parent)
	{
		Status = Info.Parent->OnChildStopped(this, Status, Info.Coroutine.Get());
		if (Status != Suspended)
		{
			FCoroutineInfo* It;
			It = m_SuspendedTasks.FindByPredicate([Target = Info.Parent](FCoroutineInfo& node) { return node.Coroutine.Get() == Target;});
			if (It)
			{
				if (Status == Running)
				{
					//reactivated task
					m_ActiveCoroutines.Add(*It);
					m_ActiveCoroutines.Last().Status = Running;
					It->Status = Aborted;
					//won't erase immediately to avoid invalidating iterator
					//but releasing task, so if another task needs to abort it, it won't confuse this one
					//with the actual valid iterator
					It->Coroutine.Reset();
				}
				else if (It->Status != Aborted) //task was already aborted... don't do anything
				{
					It->Status = Aborted;
					ProcessTaskEnd(*It, Status);
				}
			}
			else if (Status != Running)
			{
				It = ::Algo::FindByPredicate(m_ActiveCoroutines, [target = Info.Parent](FCoroutineInfo& node) { return node.Coroutine.Get() == target;});
				if (It && It->Status != Aborted) //task was already aborted, don't do anything
				{
					//mark this task for cleanup
					It->Status = Aborted;
					ProcessTaskEnd(*It, Status);
				}
			}
		}
	}
}

void ACETeam_Coroutines::FCoroutineExecutor::Cleanup()
{
	m_SuspendedTasks.RemoveAll([](FCoroutineInfo& Info) { return Info.Status == Aborted; });
}

void ACETeam_Coroutines::FCoroutineExecutor::AbortTask( FCoroutine* Coroutine )
{
	if (!Coroutine)
		return;
	FCoroutineInfo* It;
	It = m_SuspendedTasks.FindByPredicate(Coroutine_Is(Coroutine));
	if (It)
	{
		Coroutine->End(this, Aborted);
		It->Status = Aborted;
	}
	else
	{
		It = ::Algo::FindByPredicate(m_ActiveCoroutines, Coroutine_Is(Coroutine));
		if (It)
		{
			Coroutine->End(this, Aborted);
			m_ActiveCoroutines.RemoveAt(m_ActiveCoroutines.ConvertPointerToIndex(It));
		}
	}
#if !UE_BUILD_SHIPPING
	auto Criteria = [=](FCoroutineInfo& Info ) {  return Info.Parent == Coroutine && Info.Status != Aborted;};
	auto* SuspendedIt = m_SuspendedTasks.FindByPredicate(Criteria);
	FCoroutine* DependentTask = nullptr;
	if (!SuspendedIt)
	{
		auto* ActiveIt = Algo::FindByPredicate(m_ActiveCoroutines, Criteria);
		if (!ActiveIt)
		{
			return;
		}
		DependentTask = ActiveIt->Coroutine.Get();
	}
	else
	{
		DependentTask = SuspendedIt->Coroutine.Get();
	}
	check(DependentTask == nullptr);
#endif
}

void ACETeam_Coroutines::FCoroutineExecutor::ForceTaskEnd( FCoroutine* Coroutine, EStatus Status )
{
	FCoroutineInfo* It = m_SuspendedTasks.FindByPredicate(Coroutine_Is(Coroutine));
	if (It)
	{
		FCoroutineInfo t = *It;
		m_SuspendedTasks.RemoveAtSwap(It - m_SuspendedTasks.GetData());
		ProcessTaskEnd(t, Status);
	}
	else
	{
		It = ::Algo::FindByPredicate(m_ActiveCoroutines, Coroutine_Is(Coroutine));
		if (It)
		{
			FCoroutineInfo Info = *It;
			m_ActiveCoroutines.RemoveAt(m_ActiveCoroutines.ConvertPointerToIndex(It));
			ProcessTaskEnd(Info, Status);
		}
	}
}

ACETeam_Coroutines::FCoroutineExecutor::TaskFindResult ACETeam_Coroutines::FCoroutineExecutor::FindTask( FCoroutinePtr CoroutinePtr )
{
	FCoroutine* Coroutine = CoroutinePtr.Get();
	FCoroutineInfo* It = m_SuspendedTasks.FindByPredicate(Coroutine_Is(Coroutine));
	if (It)
	{
		return TFR_Suspended;
	}
	It = ::Algo::FindByPredicate(m_ActiveCoroutines, Coroutine_Is(Coroutine));
	if (It)
	{
		return TFR_Running;
	}
	return TFR_NotRunning;
}

void ACETeam_Coroutines::FCoroutineExecutor::AbortTree( FCoroutine* Coroutine )
{
	//find root
	FCoroutine* Root = Coroutine;
	for (;;)
	{
		FCoroutineInfo* It = m_SuspendedTasks.FindByPredicate(Coroutine_Is(Coroutine));
		if (It)
		{
			if (It->Parent)
			{
				Root = It->Parent;
			}
			else
			{
				Root->End(this, Aborted);
				It->Status = Aborted;
				return;
			}
		}
		else
		{
			It = ::Algo::FindByPredicate(m_ActiveCoroutines, Coroutine_Is(Coroutine));
			if (It)
			{
				if (It->Parent)
				{
					Root = It->Parent;
				}
				else
				{
					Root->End(this, Aborted);
					m_ActiveCoroutines.RemoveAt(m_ActiveCoroutines.ConvertPointerToIndex(It));
					return;
				}
			}
			else //didn't find task
			{
				//UE_LOG(LogACETeamCoroutines, Warning, TEXT("Didn't find task to abort"));
				return;
			}
		}
	}
}

//EStatus EventListenerBase::Start( FCoroutineExecutor* schd )
//{
//	m_schd = schd;
//	m_event->Register(this);
//	return Suspended;
//}

//void EventListenerBase::End( FCoroutineExecutor*, EStatus)
//{
//	m_event->Unregister(this);
//}