// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "CoroutineNode.h"

class UAudioComponent;

namespace ACETeam_Coroutines
{
	namespace Detail
	{
		struct ACETEAM_COROUTINES_API FSoundLoopNode : FCoroutineNode
		{
			FWeakObjectPtr WeakOwner;
			TFunction<UAudioComponent* ()> Lambda;
			float FadeOutTime;
			TWeakObjectPtr<UAudioComponent> SpawnedComponent;

			FSoundLoopNode(UObject* Owner, TFunction<UAudioComponent* ()> const & _Lambda, float _FadeOutTime);

			virtual EStatus Start(FCoroutineExecutor* Exec) override;
			virtual EStatus Update(FCoroutineExecutor* Exec, float dt) override;
			virtual void End(FCoroutineExecutor* Exec, EStatus Status) override;
#if WITH_ACETEAM_COROUTINE_DEBUGGER
			virtual FString Debug_GetName() const override { return TEXT("SoundLoop"); }
#endif
		};
	}

	//Calls a lambda that spawns an audio component, associated to the lifetime of an owner object,
	//The sound fades out when this branch is aborted, or when the owner ceases to be valid
	inline FCoroutineNodeRef _SoundLoop(UObject* Owner, TFunction<UAudioComponent* ()> const& Lambda, float FadeOutTime)
	{
		return MakeShared<Detail::FSoundLoopNode, DefaultSPMode>(Owner, Lambda, FadeOutTime);
	}
}