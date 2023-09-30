// Copyright ACE Team Software S.A. All Rights Reserved.

#include "CoroutinesWorldSubsystem.h"

#include "CoroutineExecutor.h"

UCoroutinesWorldSubsystem& UCoroutinesWorldSubsystem::Get(UObject* WorldContextObject)
{
	check(WorldContextObject);
	auto World = WorldContextObject->GetWorld();
	check(World);
	UCoroutinesWorldSubsystem* System = World->GetSubsystem<UCoroutinesWorldSubsystem>();
	check(System);
	return *System;
}

void UCoroutinesWorldSubsystem::StartCoroutine(ACETeam_Coroutines::FCoroutineNodeRef const& Coroutine)
{
	Executor.EnqueueCoroutine(Coroutine);
}

void UCoroutinesWorldSubsystem::AbortCoroutine(ACETeam_Coroutines::FCoroutineNodeRef const& Coroutine)
{
	Executor.AbortTree(Coroutine);
}

void UCoroutinesWorldSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (Executor.HasRemainingWork())
	{
		Executor.Step(DeltaTime);
	}
}
