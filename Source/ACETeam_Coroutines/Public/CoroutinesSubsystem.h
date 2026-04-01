// Copyright ACE Team Software S.A. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoroutineExecutor.h"
#include "Runtime/Launch/Resources/Version.h"
#include "CoroutinesSubsystem.generated.h"

/**
 * Simple subsystem that allows running coroutines from anywhere. This subsystem can be used instead of
 * UCoroutinesWorldSubsystem for logic that's not tied to the lifetime of a specific world, e.g. editor tools, or
 * for things that should run always, regardless of whether the game is paused.
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

	void StartCoroutine(ACETeam_Coroutines::FCoroutineNodeRef const& Coroutine);

private:
	ACETeam_Coroutines::FCoroutineExecutor Executor;
#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 27
	FTSTicker::FDelegateHandle TickerHandle;
#else
	FDelegateHandle TickerHandle;
#endif
};
