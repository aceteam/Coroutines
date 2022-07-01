#include "CoroutineExecutor.h"
#include "Algo/Find.h"

bool ACETeam_Coroutines::FCoroutineExecutor::SingleStep( float DeltaTime )
{
	FNodeExecInfo Info = MoveTemp(m_ActiveNodes.Last());
	m_ActiveNodes.Pop();

	if (Info.Node.Get() == nullptr)
	{
		//we've reached the marker, it's time to quit for this step
		m_ActiveNodes.AddFront(Info);
		return false;
	}
	
	if (Info.Status == Aborted)
	{
		return true;
	}

	//node is just starting, let's eval its starting condition
	if (Info.Status == None)
	{
		Info.Status = Info.Node->Start(this);

		//suspended nodes get thrown into the suspended list
		if (Info.Status == Suspended)
		{
			m_SuspendedNodes.Emplace(MoveTemp(Info));
			return true;
		}
		//check if it's already done
		if (!IsActive(Info))
		{
			ProcessNodeEnd(Info, Info.Status);
			return true;
		}
	}

	Info.Status = Info.Node->Update(this, DeltaTime);

	//suspended nodes get thrown into the suspended list
	if (Info.Status == Suspended)
	{
		m_SuspendedNodes.Emplace(MoveTemp(Info));
		return true;
	}
	if (!IsActive(Info))
	{
		ProcessNodeEnd(Info, Info.Status);
		return true;
	}
	
	m_ActiveNodes.EmplaceFront(MoveTemp(Info));
	return true;
}


ACETeam_Coroutines::FCoroutineExecutor::FCoroutineExecutor()
{
	m_ActiveNodes.Add(FNodeExecInfo()); //add empty info that serves as frame marker
}

void ACETeam_Coroutines::FCoroutineExecutor::EnqueueCoroutine(FCoroutineNodePtr const& Coroutine)
{
	EnqueueCoroutineNode(Coroutine, nullptr);
}

void ACETeam_Coroutines::FCoroutineExecutor::EnqueueCoroutineNode(FCoroutineNodePtr const& Node, FCoroutineNode* Parent)
{
	FNodeExecInfo CoroutineInfo;
	CoroutineInfo.Node = Node;
	CoroutineInfo.Parent = Parent;
	CoroutineInfo.Status = static_cast<EStatus>(None);
	m_ActiveNodes.Add(CoroutineInfo);
}

void ACETeam_Coroutines::FCoroutineExecutor::ProcessNodeEnd( FNodeExecInfo& Info, EStatus Status )
{
	Info.Node->End(this, Status);
	if (Info.Parent)
	{
		Status = Info.Parent->OnChildStopped(this, Status, Info.Node.Get());
		if (Status != Suspended)
		{
			if (FNodeExecInfo* ParentInfo = m_SuspendedNodes.FindByPredicate(NodeIs(Info.Parent)))
			{
				if (Status == Running)
				{
					//reactivated node
					m_ActiveNodes.Add(MoveTemp(*ParentInfo));
					m_ActiveNodes.Last().Status = Running;
					ParentInfo->Status = Aborted;
					//won't erase info immediately to avoid invalidating iterator
					//but node pointer was moved, so it won't be confused with
					//the actual valid iterator
					//will be erased during Cleanup
				}
				else if (ParentInfo->Status != Aborted)
				{
					ParentInfo->Status = Aborted;
					ProcessNodeEnd(*ParentInfo, Status);
				}
			}
			else if (Status != Running)
			{
				ParentInfo = ::Algo::FindByPredicate(m_ActiveNodes, NodeIs(Info.Parent));
				if (ParentInfo && ParentInfo->Status != Aborted)
				{
					//mark this node info for cleanup
					ParentInfo->Status = Aborted;
					ProcessNodeEnd(*ParentInfo, Status);
				}
			}
		}
	}
}

void ACETeam_Coroutines::FCoroutineExecutor::Cleanup()
{
	auto Pred = [](FNodeExecInfo const& Info) { return Info.Status == Aborted; };
	m_SuspendedNodes.RemoveAllSwap(Pred);
}

void ACETeam_Coroutines::FCoroutineExecutor::AbortNode( FCoroutineNode* Node )
{
	if (!Node)
		return;
	FNodeExecInfo* Info = m_SuspendedNodes.FindByPredicate(NodeIs(Node));
	if (Info)
	{
		Node->End(this, Aborted);
		Info->Status = Aborted;
	}
	else
	{
		Info = ::Algo::FindByPredicate(m_ActiveNodes, NodeIs(Node));
		if (Info)
		{
			Node->End(this, Aborted);
			m_ActiveNodes.RemoveAt(m_ActiveNodes.ConvertPointerToIndex(Info));
		}
	}
#if DO_CHECK
	auto Pred = [=](const FNodeExecInfo& _Info ) {  return _Info.Parent == Node && _Info.Status != Aborted;};
	const FNodeExecInfo* SuspendedIt = m_SuspendedNodes.FindByPredicate(Pred);
	const FCoroutineNode* DependentNode = nullptr;
	if (!SuspendedIt)
	{
		auto* ActiveIt = Algo::FindByPredicate(m_ActiveNodes, Pred);
		if (!ActiveIt)
		{
			return;
		}
		DependentNode = ActiveIt->Node.Get();
	}
	else
	{
		DependentNode = SuspendedIt->Node.Get();
	}
	check(DependentNode == nullptr);
#endif
}

void ACETeam_Coroutines::FCoroutineExecutor::ForceNodeEnd( FCoroutineNode* Node, EStatus Status )
{
	FNodeExecInfo* Info;
	Info = m_SuspendedNodes.FindByPredicate(NodeIs(Node));
	if (Info)
	{
		FNodeExecInfo t = MoveTemp(*Info);
		m_SuspendedNodes.RemoveAtSwap(Info - m_SuspendedNodes.GetData());
		ProcessNodeEnd(t, Status);
	}
	else
	{
		Info = ::Algo::FindByPredicate(m_ActiveNodes, NodeIs(Node));
		if (Info)
		{
			FNodeExecInfo TempInfo = MoveTemp(*Info);
			m_ActiveNodes.RemoveAt(m_ActiveNodes.ConvertPointerToIndex(Info));
			ProcessNodeEnd(TempInfo, Status);
		}
	}
}

ACETeam_Coroutines::FCoroutineExecutor::EFindNodeResult ACETeam_Coroutines::FCoroutineExecutor::FindCoroutineNode(FCoroutineNodePtr const& CoroutinePtr)
{
	FCoroutineNode* Coroutine = CoroutinePtr.Get();
	FNodeExecInfo* It = m_SuspendedNodes.FindByPredicate(NodeIs(Coroutine));
	if (It)
	{
		return EFindNodeResult::Suspended;
	}
	It = ::Algo::FindByPredicate(m_ActiveNodes, NodeIs(Coroutine));
	if (It)
	{
		return It->Status == Aborted ? EFindNodeResult::Aborted : EFindNodeResult::Running;
	}
	return EFindNodeResult::NotRunning;
}

void ACETeam_Coroutines::FCoroutineExecutor::AbortTree( FCoroutineNode* Coroutine )
{
	//find root
	FCoroutineNode* Root = Coroutine;
	for (;;)
	{
		FNodeExecInfo* It = m_SuspendedNodes.FindByPredicate(NodeIs(Coroutine));
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
			It = ::Algo::FindByPredicate(m_ActiveNodes, NodeIs(Coroutine));
			if (It)
			{
				if (It->Parent)
				{
					Root = It->Parent;
				}
				else
				{
					Root->End(this, Aborted);
					m_ActiveNodes.RemoveAt(m_ActiveNodes.ConvertPointerToIndex(It));
					return;
				}
			}
			else //didn't find node
			{
				//UE_LOG(LogACETeamCoroutines, Warning, TEXT("Didn't find node to abort"));
				return;
			}
		}
	}
}