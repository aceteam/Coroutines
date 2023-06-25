// Copyright ACE Team Software S.A. All Rights Reserved.

#include "CoroutineGameplayUtils.h"

#include "Components/AudioComponent.h"

ACETeam_Coroutines::Detail::FSoundLoopNode::FSoundLoopNode(UObject* Owner, TFunction<UAudioComponent*()> const& _Lambda,
	float _FadeOutTime): WeakOwner(Owner)
						, Lambda(_Lambda)
						, FadeOutTime(_FadeOutTime)
{}

ACETeam_Coroutines::EStatus ACETeam_Coroutines::Detail::FSoundLoopNode::Start(FCoroutineExecutor* Exec)
{
	if (WeakOwner.IsValid())
	{
		SpawnedComponent = Lambda();
		if (ensure(SpawnedComponent.IsValid()))
		{
			//This node is only supposed to be used with looping sounds
			check(SpawnedComponent->Sound->IsLooping());
		}
		return SpawnedComponent != nullptr ? Suspended : Failed;
	}
	return Failed;
}

void ACETeam_Coroutines::Detail::FSoundLoopNode::End(FCoroutineExecutor* Exec, EStatus Status)
{
	if (SpawnedComponent.IsValid())
	{
		SpawnedComponent->bAutoDestroy = true;
		SpawnedComponent->FadeOut(FadeOutTime, 0.0f);
	}
}
