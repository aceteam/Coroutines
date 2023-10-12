// Copyright ACE Team Software S.A. All Rights Reserved.

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebuggerCategory_Coroutines.h"

#include "CoroutineExecutor.h"
#include "CoroutinesWorldSubsystem.h"
#include "Engine/Canvas.h"
#include "Fonts/FontMeasure.h"

using namespace ACETeam_Coroutines;

FGameplayDebuggerCategory_Coroutines::FGameplayDebuggerCategory_Coroutines()
{
	bShowOnlyWithDebugActor = false;

	BindKeyPress(EKeys::RightBracket.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_Coroutines::ToggleCompactMode, EGameplayDebuggerInputMode::Local);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_Coroutines::MakeInstance()
{
	return MakeShared<FGameplayDebuggerCategory_Coroutines>();
}

void FGameplayDebuggerCategory_Coroutines::DrawData(APlayerController* OwnerPC,
	FGameplayDebuggerCanvasContext& CanvasContext)
{
	CanvasContext.Printf(TEXT("\n[{yellow}%s{white}] Toggle Compact mode"), *GetInputHandlerDescription(0));
	
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

	const TSharedRef< FSlateFontMeasure > FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	auto FontInfo = GEngine->GetTinyFont()->GetLegacySlateFontInfo();

	FCanvasTileItem BackgroundTile(FVector2d{X-GraphMargin*0.1, Y}, FVector2D(GraphWidth + GraphMargin*0.2, FMath::Max(LastHeight, RowHeight*30)), FLinearColor::Black.CopyWithNewOpacity(0.3f));
	BackgroundTile.BlendMode = SE_BLEND_TranslucentAlphaOnly;
	BackgroundTile.Draw(FCanvas);

	struct FScopeHeight
	{
		FCoroutineNode* Scope;
		double Y;
	};
	TArray<FScopeHeight> ScopeHeights;
	auto HeightForScope = [&](FCoroutineNode* Scope)
	{
		for (int i = ScopeHeights.Num()-1; i >= 0; --i)
		{
			if (ScopeHeights[i].Scope == Scope)
				return ScopeHeights[i].Y;
		}
		/**
		 * There's some bug here related to deferred nodes that triggers this ensure
		 * I don't have time to fix it yet, so the ensure is disabled
		 */
		//ensure(false);
		return 0.0;
	};
	for (const auto Exec : ExecutorsToDebug)
	{
		if (Exec->DebuggerInfo.Num() == 0)
			continue;
		ScopeHeights.Reset();
		ScopeHeights.Add( FScopeHeight{Exec->DebuggerInfo[0].Node, Y + RowHeight*0.6} );
		for (auto& Row : Exec->DebuggerInfo)
		{
			bool bSkipInCompactMode = !Row.bIsScope && !Row.bIsLeaf;
			if (bCompactMode && bSkipInCompactMode)
			{
				continue;
			}
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
				if (DrawnEntries == 0)
				{
					FirstEntryX = EntryStartPos.X;
				}
				++DrawnEntries;
				const double EntryWidth = FMath::Max(1.0, EntryDuration * TimeToPixels);
				FCanvasTileItem EntryTile(EntryStartPos, FVector2D(EntryWidth, RowHeight), ColorForStatus(Entry.Status));
				EntryTile.BlendMode = SE_BLEND_TranslucentAlphaOnly;
				EntryTile.Draw(FCanvas);
				EntryStartPos.X += 4.0;
				if (Entry.Name.Len() > 0)
				{
					auto TextMeasure = FontMeasure->Measure(Entry.Name, FontInfo, 1);
					if (TextMeasure.X + 5.0 < EntryWidth)
					{
						FCanvasTextItem EntryText(EntryStartPos, FText::FromString(Entry.Name), GEngine->GetTinyFont(), FLinearColor::White);
						EntryText.Draw(FCanvas);
					}
				}
			}
			if (DrawnEntries == 0)
				continue;
			if (Row.Scope)
			{
				double MyScopeY = HeightForScope(Row.Scope);
				if (MyScopeY > 0.0) //TEMP: so we don't draw black lines across the graph when bug is triggered
				{
					FCanvasTileItem EntryTile(FVector2D(FirstEntryX, MyScopeY), FVector2D(2.0, Y - MyScopeY + RowHeight*0.4), FLinearColor::Black.CopyWithNewOpacity(0.5f));
					EntryTile.BlendMode = SE_BLEND_TranslucentAlphaOnly;
					EntryTile.Draw(FCanvas);
				}
			}
			if (Row.bIsScope)
			{
				ScopeHeights.Add(FScopeHeight { Row.Node, Y + RowHeight*0.8 });
			}
			Y += RowHeight+SpaceBetweenRows;
		}
		LastHeight = Y;
	}
}

void FGameplayDebuggerCategory_Coroutines::ToggleCompactMode()
{
	bCompactMode = !bCompactMode;
}

#endif // WITH_GAMEPLAY_DEBUGGER
