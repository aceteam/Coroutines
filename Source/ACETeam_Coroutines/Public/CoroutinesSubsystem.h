// Copyright ACE Team Software S.A. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoroutineExecutor.h"
#include "CoroutineNode.h"
#include "Launch/Resources/Version.h"
#include "CoroutinesSubsystem.generated.h"

/**
 * Simple subsystem that allows running coroutines from anywhere
 * Usage example: UCoroutinesSubsystem::Get().StartCoroutine(<coroutine>);
 *
 * Ticks only while a coroutine is active, and steps through them on the main thread.
 */
UCLASS()
class ACETEAM_COROUTINES_API UCoroutinesSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
	
public:
	static UCoroutinesSubsystem& Get();

	void StartCoroutine(ACETeam_Coroutines::FCoroutineNodePtr const& Coroutine);

private:
	ACETeam_Coroutines::FCoroutineExecutor Executor;
#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 27
	FTSTicker::FDelegateHandle TickerHandle;
#else
	FDelegateHandle TickerHandle;
#endif
};
