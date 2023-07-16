// Copyright Daniel Amthauer. All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "CoroutineEvents.h"
#include "UObject/Object.h"
#include "CoroutineEventBPWrapper.generated.h"

/**
 * Simple wrapper to expose coroutine events to Blueprint.
 * Events with arguments could be exposed in a similar way, but you'd need a specific type for each combination of argument types
 */
UCLASS()
class ACETEAM_COROUTINES_API UCoroutineEventBPWrapper : public UObject
{
	GENERATED_BODY()

	ACETeam_Coroutines::TEventWeakPtr<void> EventPtr;

public:

	UFUNCTION(BlueprintCallable)
	void FireEvent();

	static UCoroutineEventBPWrapper* MakeFromEvent(ACETeam_Coroutines::TEventRef<void> EventRef);
};
