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
			ensure(SpawnedComponent->Sound->IsLooping());
		}
		return SpawnedComponent != nullptr ? Running : Failed;
	}
	return Failed;
}

ACETeam_Coroutines::EStatus ACETeam_Coroutines::Detail::FSoundLoopNode::Update(FCoroutineExecutor* Exec, float dt)
{
	if (WeakOwner.IsStale() || SpawnedComponent.IsStale())
		return Failed;
	return Running;
}

void ACETeam_Coroutines::Detail::FSoundLoopNode::End(FCoroutineExecutor* Exec, EStatus Status)
{
	if (SpawnedComponent.IsValid())
	{
		SpawnedComponent->bAutoDestroy = true;
		SpawnedComponent->FadeOut(FadeOutTime, 0.0f);
	}
}

#if WITH_ACETEAM_COROUTINE_DEBUGGER
FString ACETeam_Coroutines::Detail::FSoundLoopNode::Debug_GetName() const
{
	if (auto AudioComponent = SpawnedComponent.Get())
	{
#if ENGINE_MAJOR_VERSION < 5
		auto SoundBase = AudioComponent->Sound;
#else
		auto SoundBase = AudioComponent->GetSound();
#endif
		if (!SoundBase)
		{
			return TEXT("SoundLoop");
		}
		return FString::Printf(TEXT("SoundLoop: %s"), *SoundBase->GetName());
	}
	return TEXT("SoundLoop");
}
#endif
