#include "CoroutineStreaming.h"

#include "CoroutineExecutor.h"
#include "Engine/AssetManager.h"

ACETeam_Coroutines::Detail::FAssetStreamingNode::FAssetStreamingNode(TFunction<TArray<FSoftObjectPath>()> const& InGetter, TAsyncLoadPriority InAsyncLoadPriority)
:SoftObjectPathGetter(InGetter)
,AsyncLoadPriority(InAsyncLoadPriority)
{
}

ACETeam_Coroutines::EStatus ACETeam_Coroutines::Detail::FAssetStreamingNode::Start(FCoroutineExecutor* Exec)
{
	const auto SoftObjectPaths = SoftObjectPathGetter();
	if (SoftObjectPaths.Num() == 0)
		return Completed;
	CachedExec = Exec;
	Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftObjectPaths, FStreamableDelegate::CreateLambda([this, Weak = AsWeak()]
	{
		if (!Weak.IsValid() || Handle->WasCanceled())
			return;
		if (CachedExec)
		{
			CachedExec->ForceNodeEnd(this, Completed);
		}
	}), AsyncLoadPriority, false, false, TEXT("Coroutine"));
	if (!Handle.IsValid())
		return Failed;
	return Suspended;
}

void ACETeam_Coroutines::Detail::FAssetStreamingNode::End(FCoroutineExecutor* Exec, EStatus Status)
{
	if (Status == Aborted)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
			Handle.Reset();
		}
	}
	CachedExec = nullptr;
}

ACETeam_Coroutines::FCoroutineNodeRef ACETeam_Coroutines::_StreamAssets(TArray<FSoftObjectPath> const& SoftObjectPaths, TAsyncLoadPriority AsyncLoadPriority)
{
	return MakeShared<Detail::FAssetStreamingNode, DefaultSPMode>([=]{ return SoftObjectPaths; }, AsyncLoadPriority);
}

ACETeam_Coroutines::FCoroutineNodeRef ACETeam_Coroutines::_StreamAssets(
	std::initializer_list<FSoftObjectPath> SoftObjectPaths, TAsyncLoadPriority AsyncLoadPriority)
{
	return MakeShared<Detail::FAssetStreamingNode, DefaultSPMode>([Array = TArray(SoftObjectPaths)]{ return Array; }, AsyncLoadPriority);
}

ACETeam_Coroutines::FCoroutineNodeRef ACETeam_Coroutines::_StreamAssets(
	TFunction<TArray<FSoftObjectPath>()> SoftObjectPathsGetter, TAsyncLoadPriority AsyncLoadPriority)
{
	return MakeShared<Detail::FAssetStreamingNode, DefaultSPMode>(SoftObjectPathsGetter, AsyncLoadPriority);
}
