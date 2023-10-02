// Copyright ACE Team Software S.A. All Rights Reserved.

#include "GameplayDebuggerCategory_Coroutines.h"

#include "CoroutineExecutor.h"
#include "CoroutinesWorldSubsystem.h"
#include "Engine/Canvas.h"

using namespace ACETeam_Coroutines;

FGameplayDebuggerCategory_Coroutines::FGameplayDebuggerCategory_Coroutines()
{
	bShowOnlyWithDebugActor = false;
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
	const float GraphWidth = 500.0f;
	const float GraphMargin = 50.0f;
	const float RowHeight = 16.0f;
	const float SpaceBetweenRows = 1.0f;
	const float InitialY = 200.0;
	const float X = Canvas->SizeX - GraphWidth - GraphMargin;
	float Y = InitialY;
	
	const float OneOverGraphTimeWindow = 1.0f / GraphTimeWindow;
	const float TimeToPixels = OneOverGraphTimeWindow*GraphWidth;

	auto ColorForStatus = [] (EStatus Status)
	{
		const float Opacity = 0.5f;
		switch (Status)
		{
		case Completed:
			return FLinearColor(0.0f, 1.0f, 0.0f, Opacity);
		case Suspended:
			return FLinearColor(1.0f, 1.0f, 0.0f, Opacity);
		case Failed:
			return FLinearColor(1.0f, 0.0f, 0.0f, Opacity);
		case Running:
			return FLinearColor(0.0f, 0.0f, 1.0f, Opacity);
		case Aborted:
			return FLinearColor(1.0f, 0.5f, 0.0f, Opacity);
		default:
			return FLinearColor(1.0f, 1.0f, 1.0f, Opacity);
		}
	};

	double LastRootY = Y + RowHeight*0.8;
	for (const auto Exec : ExecutorsToDebug)
	{
		for (auto& Row : Exec->DebuggerInfo)
		{
			const auto* FirstEntryToRender = Row.Entries.FindByPredicate([&](const FCoroutineExecutor::FDebuggerEntry& Entry)
			{
				return Entry.EndTime < 0.0 || Entry.EndTime >= StartTime;
			});
			if (FirstEntryToRender == nullptr)
			{
				continue;
			}
			const int FirstEntryIndex = FirstEntryToRender - Row.Entries.GetData();
			int DrawnEntries = 0;
			double FirstEntryX = 0.0;
			for (int Index = FirstEntryIndex; Index < Row.Entries.Num(); ++Index)
			{
				const auto& Entry = Row.Entries[Index];
				const double CappedStartTime = FMath::Max(StartTime, Entry.StartTime);
				FVector2D EntryStartPos(X + (CappedStartTime - StartTime) * TimeToPixels, Y);
				const double CurrentEndTime = Entry.EndTime < 0.0 ? CurrentTime : Entry.EndTime;
				const double EntryDuration = CurrentEndTime - CappedStartTime;
				if (CurrentEndTime - Entry.StartTime < 0.01)
				{
					continue;
				}
				if (DrawnEntries == 0)
				{
					FirstEntryX = EntryStartPos.X;
				}
				++DrawnEntries;
				const double EntryWidth = EntryDuration * TimeToPixels;
				FCanvasTileItem EntryTile(EntryStartPos, FVector2D(EntryWidth, RowHeight), ColorForStatus(Entry.Status));
				EntryTile.BlendMode = SE_BLEND_TranslucentAlphaOnly;
				EntryTile.Draw(FCanvas);
				EntryStartPos.X += 4.0;
				FCanvasTextItem EntryText(EntryStartPos, FText::FromString(Entry.Name), GEngine->GetSmallFont(), FLinearColor::White);
				EntryText.Draw(FCanvas);
			}
			if (DrawnEntries == 0)
				continue;
			if (Row.Root != Row.Node)
			{
				FCanvasTileItem EntryTile(FVector2D(FirstEntryX, LastRootY), FVector2D(2.0, Y - LastRootY), FLinearColor::Black.CopyWithNewOpacity(0.5f));
				EntryTile.BlendMode = SE_BLEND_TranslucentAlphaOnly;
				EntryTile.Draw(FCanvas);
			}
			else
			{
				LastRootY = Y + RowHeight*0.8;
			}
			Y += RowHeight+SpaceBetweenRows;
		}
	}
}
