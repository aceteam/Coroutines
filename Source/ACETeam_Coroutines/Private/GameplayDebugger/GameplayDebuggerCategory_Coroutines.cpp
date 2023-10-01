// Copyright ACE Team Software S.A. All Rights Reserved.

#include "GameplayDebuggerCategory_Coroutines.h"

FGameplayDebuggerCategory_Coroutines::FGameplayDebuggerCategory_Coroutines()
{
	
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_Coroutines::MakeInstance()
{
	return MakeShared<FGameplayDebuggerCategory_Coroutines>();
}

void FGameplayDebuggerCategory_Coroutines::DrawData(APlayerController* OwnerPC,
	FGameplayDebuggerCanvasContext& CanvasContext)
{
	if (UCanvas* Canvas = CanvasContext.Canvas.Get())
	{
	}
}
