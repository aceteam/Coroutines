// Copyright ACE Team Software S.A. All Rights Reserved.


#include "CoroutinesSubsystem.h"

#include "CoroutineExecutor.h"

UCoroutinesSubsystem& UCoroutinesSubsystem::Get()
{
	UCoroutinesSubsystem* System = GEngine->GetEngineSubsystem<UCoroutinesSubsystem>();
	check(System);
	return *System;
}

void UCoroutinesSubsystem::StartCoroutine(ACETeam_Coroutines::FCoroutineNodePtr const& Coroutine)
{
	Executor.EnqueueCoroutine(Coroutine);
	if (!TickerHandle.IsValid())
	{
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [=](float DeltaSeconds)
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
