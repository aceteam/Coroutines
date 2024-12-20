// Copyright ACE Team Software S.A. All Rights Reserved.

#include "CoroutinesWorldSubsystem.h"

#include "CoroutineExecutor.h"

UCoroutinesWorldSubsystem& UCoroutinesWorldSubsystem::Get(const UObject* WorldContextObject)
{
	check(WorldContextObject);
	auto World = WorldContextObject->GetWorld();
	check(World);
	UCoroutinesWorldSubsystem* System = World->GetSubsystem<UCoroutinesWorldSubsystem>();
	check(System);
	return *System;
}

#if WITH_ACETEAM_COROUTINE_DEBUGGER
bool GEnsureCoroutinesAreNamed = false;
FAutoConsoleVariableRef EnsureCoroutinesAreNamedCVar (TEXT("ace.EnsureCoroutinesAreNamed"), GEnsureCoroutinesAreNamed, TEXT("If this is on, an ensure will be triggered if a non-named scope is added to the subsystem directly"));
#endif

void UCoroutinesWorldSubsystem::StartCoroutine(ACETeam_Coroutines::FCoroutineNodeRef const& Coroutine)
{
#if WITH_ACETEAM_COROUTINE_DEBUGGER
	if (GEnsureCoroutinesAreNamed)
	{
		ensureAlways(Coroutine->Debug_IsDebuggerScope());
	}
#endif
	Executor.EnqueueCoroutine(Coroutine);
}

void UCoroutinesWorldSubsystem::AbortCoroutine(ACETeam_Coroutines::FCoroutineNodeRef const& Coroutine)
{
	Executor.AbortTree(Coroutine);
}

void UCoroutinesWorldSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TRACE_CPUPROFILER_EVENT_SCOPE(UCoroutinesWorldSubsystem::ExecutorStep);
	
	if (Executor.HasRemainingWork())
	{
		Executor.Step(DeltaTime);
	}
}
