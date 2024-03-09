// Copyright ACE Team Software S.A. All Rights Reserved.

#pragma once

#include "CoroutineExecutor.h"
#include "CoroutineNode.h"
#include "CoroutinesWorldSubsystem.generated.h"

/**
 * Simple subsystem that allows running coroutines in a world
 * Usage example: UCoroutinesWorldSubsystem::Get(<world context obj>).StartCoroutine(<coroutine>);
 *
 * Steps through coroutines on the game thread while the game is not paused.
 */
UCLASS()
class ACETEAM_COROUTINES_API UCoroutinesWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
public:
	static UCoroutinesWorldSubsystem& Get(const UObject* WorldContextObject);

	void StartCoroutine(ACETeam_Coroutines::FCoroutineNodeRef const& Coroutine);

	void AbortCoroutine(ACETeam_Coroutines::FCoroutineNodeRef const& Coroutine);

	virtual void Tick(float DeltaTime) override;

	TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCoroutinesWorldSubsystem, STATGROUP_Tickables); }

private:
	ACETeam_Coroutines::FCoroutineExecutor Executor;

#if WITH_ACETEAM_COROUTINE_DEBUGGER
	friend class FGameplayDebuggerCategory_Coroutines;
#endif
};
