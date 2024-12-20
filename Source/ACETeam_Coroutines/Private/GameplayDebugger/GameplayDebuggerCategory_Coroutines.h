// Copyright ACE Team Software S.A. All Rights Reserved.

#pragma once

#if WITH_ACETEAM_COROUTINE_DEBUGGER

#include "GameplayDebuggerCategory.h"

class APlayerController;

class FGameplayDebuggerCategory_Coroutines : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_Coroutines();

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

	void ToggleCompactMode();
	void ScrollDown();
	void ScrollUp();

	double GraphTimeWindow = 10.0;

	double LastHeight = 0.0;

	bool bCompactMode = true;
	int Offset = 0;
};

#endif // WITH_ACETEAM_COROUTINE_DEBUGGER
