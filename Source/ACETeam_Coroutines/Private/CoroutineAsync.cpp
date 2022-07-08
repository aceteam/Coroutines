// Copyright ACE Team Software S.A. All Rights Reserved.
#include "CoroutineAsync.h"

#include "Engine/AssetManager.h"

namespace ACETeam_Coroutines
{
	namespace Detail
	{
		EStatus FAsyncObjectLoadNode::Start(FCoroutineExecutor* Exec)
		{
			StreamableHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(ObjectsToLoad, FStreamableDelegate::CreateLambda([=, SharedThis = AsShared()]
			{
				HandleLoaded();
				if (CachedExec)
				{
					CachedExec->ForceNodeEnd(this, Completed);
				}
			}));
			if (!StreamableHandle.IsValid())
			{
				UE_DEBUG_BREAK();
				return Failed;
			}
			return Suspended;
		}

		void FAsyncObjectLoadNode::End(FCoroutineExecutor* Exec, EStatus Status)
		{
			if (Status == Aborted)
			{
				if (StreamableHandle.IsValid())
				{
					StreamableHandle->CancelHandle();
				}
			}
			CachedExec = nullptr;
			StreamableHandle.Reset();
		}
	}

	FCoroutineNodeRef _AsyncLoadObjects(TArrayView<const TSoftObjectPtr<>> SoftObjects)
	{
		auto Paths = Detail::ConvertToSoftObjectPaths(SoftObjects);
		return MakeShared<Detail::FAsyncObjectLoadNode, DefaultSPMode>(MoveTemp(Paths));
	}

	TArray<FSoftObjectPath> Detail::ConvertToSoftObjectPaths(TArrayView<const TSoftObjectPtr<>> SoftObjectPtrs)
	{
		TArray<FSoftObjectPath> Paths;
		Paths.Reserve(SoftObjectPtrs.Num());
		for (auto& Ptr : SoftObjectPtrs)
		{
			Paths.Add(Ptr.ToSoftObjectPath());
		}
		return Paths;
	}
}
