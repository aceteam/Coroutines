// Copyright ACE Team Software S.A. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoroutineExecutor.h"
#include "CoroutineNode.h"
#include "CoroutinesSubsystem.generated.h"

/**
 * Simple subsystem that allows running coroutines from anywhere
 * Usage example: UCoroutinesSubsystem::Get().AddCoroutine(<coroutine>);
 *
 * Ticks only while a coroutine is active, and steps through them on the main thread.
 */
UCLASS()
class ACETEAM_COROUTINES_API UCoroutinesSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
	
public:
	static UCoroutinesSubsystem& Get();

	void AddCoroutine(ACETeam_Coroutines::FCoroutineNodePtr const& Coroutine);

private:
	ACETeam_Coroutines::FCoroutineExecutor Executor;
	FTSTicker::FDelegateHandle TickerHandle;
};
