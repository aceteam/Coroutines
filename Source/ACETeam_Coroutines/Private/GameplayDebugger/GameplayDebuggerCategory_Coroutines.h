// Copyright ACE Team Software S.A. All Rights Reserved.

#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebuggerCategory.h"

class APlayerController;

class FGameplayDebuggerCategory_Coroutines : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_Coroutines();

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

	double GraphTimeWindow = 30.0;
};

#endif // WITH_GAMEPLAY_DEBUGGER
