// Copyright ACE Team Software S.A. All Rights Reserved.

#include "GameplayDebuggerCategory_Coroutines.h"

#include "CoroutineExecutor.h"
#include "CoroutinesWorldSubsystem.h"
#include "Engine/Canvas.h"

using namespace ACETeam_Coroutines;

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
	UCanvas* Canvas = CanvasContext.Canvas.Get();
	if (!Canvas)
		return;
	FCanvas* FCanvas = Canvas->Canvas;
	FCoroutineExecutor* ExecutorsToDebug [] = {
		&UCoroutinesWorldSubsystem::Get(OwnerPC).Executor
	};
	const double CurrentTime = FApp::GetCurrentTime();
	const double StartTime = CurrentTime - GraphTimeWindow;
	const float GraphWidth = 800.0f;
	const float GraphMargin = 100.0f;
	const float RowHeight = 50.0f;
	const float SpaceBetweenRows = 1.0f;
	float X = Canvas->SizeX - GraphWidth - GraphMargin;
	float Y = 250.0f;
	const float OneOverGraphTimeWindow = 1.0f / GraphTimeWindow;

	auto ColorForStatus = [] (EStatus Status)
	{
		switch (Status)
		{
		case Completed:
			return FLinearColor(0.0f, 1.0f, 0.0f, 0.3f);
		case Suspended:
			return FLinearColor(1.0f, 1.0f, 0.0f, 0.3f);
		case Failed:
			return FLinearColor(1.0f, 0.0f, 0.0f, 0.3f);
		case Running:
			return FLinearColor(0.0f, 0.0f, 1.0f, 0.3f);
		default:
			return FLinearColor(1.0f, 1.0f, 1.0f, 0.3f);
		}
	};
	
	for (const auto Exec : ExecutorsToDebug)
	{
		for (auto& Row : Exec->DebuggerInfo)
		{
			const auto* FirstEntryToRender = Row.Entries.FindByPredicate([&](const FCoroutineExecutor::FDebuggerEntry& Entry)
			{
				return Entry.EndTime < 0.0f || Entry.EndTime >= StartTime;
			});
			const int FirstEntryIndex = FirstEntryToRender - Row.Entries.GetData();
			for (int Index = FirstEntryIndex; Index < Row.Entries.Num(); ++Index)
			{
				const auto& Entry = Row.Entries[Index];
				const FVector2D EntryStartPos((Entry.StartTime - StartTime) * OneOverGraphTimeWindow, Y);
				const double CurrentEndTime = Entry.EndTime < 0.0f ? CurrentTime : Entry.EndTime;
				const double EntryDuration = CurrentEndTime - Entry.StartTime;
				const double EntryWidth = EntryDuration * OneOverGraphTimeWindow;
				FCanvasTileItem EntryTile(EntryStartPos, FVector2D(EntryWidth, RowHeight), ColorForStatus(Entry.Status));
				EntryTile.BlendMode = SE_BLEND_TranslucentAlphaOnly;
				EntryTile.Draw(FCanvas);
			}
			Y += RowHeight+SpaceBetweenRows;
		}
	}
}
