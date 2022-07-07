// Copyright ACE Team Software S.A. All Rights Reserved.


#include "CoroutinesSubsystem.h"

#include "CoroutineExecutor.h"

UCoroutinesSubsystem& UCoroutinesSubsystem::Get()
{
	UCoroutinesSubsystem* System = GEngine->GetEngineSubsystem<UCoroutinesSubsystem>();
	check(System);
	return *System;
}

void UCoroutinesSubsystem::StartCoroutine(ACETeam_Coroutines::FCoroutineNodeRef const& Coroutine)
{
	Executor.EnqueueCoroutine(Coroutine);
	if (!TickerHandle.IsValid())
	{
#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 27
		TickerHandle = FTSTicker::GetCoreTicker()
#else
		TickerHandle = FTicker::GetCoreTicker()
#endif
		.AddTicker(FTickerDelegate::CreateWeakLambda(this, [=](float DeltaSeconds)
		{
			Executor.Step(DeltaSeconds);
			if (Executor.HasRemainingWork())
			{
				return true;
			}
			TickerHandle.Reset();
			return false;
		}));
	}
}
