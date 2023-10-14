// Copyright ACE Team Software S.A. All Rights Reserved.
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

		TrackNodeStart(Info.Node.Get(), Info.Parent, Info.Status);

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
		TrackNodeSuspendFromUpdate(Info.Node.Get());
		
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

ACETeam_Coroutines::FCoroutineExecutor::~FCoroutineExecutor()
{
	if (HasRemainingWork())
	{
		TArray<FCoroutineNodePtr> ParentNodes;
		for (auto& Info : m_SuspendedNodes)
		{
			if (Info.Parent == nullptr)
			{
				ParentNodes.Add(Info.Node);
			}
		}
		for (auto& ParentNode : ParentNodes)
		{
			AbortNode(ParentNode.Get());
		}
		Cleanup();
		
		ParentNodes.Reset();
		for (auto& Info : m_ActiveNodes)
		{
			if (Info.Parent == nullptr && Info.Node.IsValid())
			{
				ParentNodes.Add(Info.Node);
			}
		}
		for (auto& ParentNode : ParentNodes)
		{
			AbortNode(ParentNode.Get());
		}
		check(!HasRemainingWork());
	}
}

void ACETeam_Coroutines::FCoroutineExecutor::EnqueueCoroutine(FCoroutineNodeRef const& Coroutine)
{
	EnqueueCoroutineNode(Coroutine, nullptr);
}

void ACETeam_Coroutines::FCoroutineExecutor::EnqueueCoroutineNode(FCoroutineNodeRef const& Node, FCoroutineNode* Parent)
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
	TrackNodeEnd(Info.Node.Get(), Status);
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

#if WITH_GAMEPLAY_DEBUGGER
	const double DropTime = FApp::GetCurrentTime() - 60.0f;
	for (auto RowIt = DebuggerInfo.CreateIterator(); RowIt; ++RowIt)
	{
		for (auto It = RowIt->Entries.CreateIterator(); It; ++It)
		{
			if (It->EndTime < 0.0f)
				continue;
			if (It->EndTime < DropTime)
			{
				It.RemoveCurrent();
			}
		}
		if (RowIt->Entries.Num() == 0)
		{
			RowIt.RemoveCurrent();
		}
	}
#endif
}

void ACETeam_Coroutines::FCoroutineExecutor::AbortNode( FCoroutineNode* Node )
{
	if (!Node)
		return;
	FNodeExecInfo* Info = m_SuspendedNodes.FindByPredicate(NodeIs(Node));
	if (Info)
	{
		Node->End(this, Aborted);
		TrackNodeEnd(Node, Aborted);
		Info->Status = Aborted;
	}
	else
	{
		Info = ::Algo::FindByPredicate(m_ActiveNodes, NodeIs(Node));
		if (Info)
		{
			Node->End(this, Aborted);
			TrackNodeEnd(Node, Aborted);
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
	check(IsFinished(Status));
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

void ACETeam_Coroutines::FCoroutineExecutor::TrackNodeStart(FCoroutineNode* Node, FCoroutineNode* Parent, EStatus Status)
{
#if WITH_GAMEPLAY_DEBUGGER
	FDebuggerRow* RowForNode = DebuggerInfo.FindByKey(Node);
	if (!RowForNode)
	{
		int IndexToInsert = DebuggerInfo.Num();
		FCoroutineNode* Scope = nullptr;
		int Depth = 0;
		if (Parent)
		{
			FDebuggerRow* RowForParent = DebuggerInfo.FindByKey(Parent);
			Depth = RowForParent->Depth + 1;
			check(RowForParent);
			RowForParent->bIsLeaf = false;
			const int IndexForParent = RowForParent - DebuggerInfo.GetData();
			IndexToInsert = IndexForParent + 1;
			Scope = RowForParent->bIsScope ? Parent : RowForParent->Scope;
			if (RowForParent->bIsDeferredNodeGenerator)
			{
				for (int i = IndexToInsert; i < DebuggerInfo.Num(); ++i)
				{
					if (DebuggerInfo[i].Parent == Parent)
					{
						RowForNode = &DebuggerInfo[i];
						RowForNode->Node = Node;
						goto RowAssigned;
					}
				}
			}
		}
		RowForNode = &DebuggerInfo.Insert_GetRef(
			FDebuggerRow{
				Node, Parent, Scope, Depth, Node->Debug_IsDeferredNodeGenerator(), Parent == nullptr || Node->Debug_IsDebuggerScope(), true
			}, IndexToInsert);
	}
RowAssigned:
	if (RowForNode->Entries.Num() > 0)
	{
		double CurrentTime = FApp::GetCurrentTime();
		FDebuggerEntry& LastEntry = RowForNode->Entries.Last();
		//coalesce very short entries, if they happened recently
		if ((LastEntry.EndTime - LastEntry.StartTime) < 0.03 && (CurrentTime - LastEntry.EndTime) < 0.03)
		{
			LastEntry.Name = Node->Debug_GetName();
			LastEntry.Status = Status;
			LastEntry.EndTime = -1.0;
			return;
		}
	}
	RowForNode->Entries.Add(FDebuggerEntry{Node->Debug_GetName(), Status, FApp::GetCurrentTime()});
#endif
}

void ACETeam_Coroutines::FCoroutineExecutor::TrackNodeSuspendFromUpdate(FCoroutineNode* Node)
{
#if WITH_GAMEPLAY_DEBUGGER
	FDebuggerRow* RowForNode = DebuggerInfo.FindByKey(Node);
	check(RowForNode);
	check(RowForNode->Entries.Num() > 0);
	RowForNode->Entries.Last().Status = Suspended;
#endif
}

void ACETeam_Coroutines::FCoroutineExecutor::TrackNodeEnd(FCoroutineNode* Node, EStatus Status)
{
#if WITH_GAMEPLAY_DEBUGGER
	FDebuggerRow* RowForNode = DebuggerInfo.FindByKey(Node);
	if (RowForNode)
	{
		if (RowForNode->Entries.Num() > 0)
		{
			RowForNode->Entries.Last().EndTime = FApp::GetCurrentTime();
		}
	}
#endif
}

ACETeam_Coroutines::FCoroutineExecutor::EFindNodeResult ACETeam_Coroutines::FCoroutineExecutor::FindCoroutineNode(FCoroutineNodeRef const& CoroutinePtr)
{
	FCoroutineNode* Coroutine = &CoroutinePtr.Get();
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
				TrackNodeEnd(Root, Aborted);
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
					TrackNodeEnd(Root, Aborted);
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