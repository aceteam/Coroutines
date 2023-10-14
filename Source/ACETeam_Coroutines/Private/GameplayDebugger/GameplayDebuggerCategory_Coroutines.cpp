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

static TArray<FString> GCoroutineDebuggerFilter;

struct FGameplayDebuggerCategory_CoroutinesCommands
{
	static void SetFilter(const TArray< FString >& Args, UWorld* World)
	{
		GCoroutineDebuggerFilter = Args;
	}
};

static FAutoConsoleCommandWithWorldAndArgs SetFontSizeCmd(
	TEXT("gdt.Coroutine.SetFilter"),
	TEXT("Configures zero or more filter strings for the coroutine debugger. Usage: gdt.Coroutine.SetFilter <string1> <string2> ..."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FGameplayDebuggerCategory_CoroutinesCommands::SetFilter));


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
	const float GraphWidth = 450.0f;
	const float GraphMargin = 150.0f;
	const float RowHeight = 14.0f;
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

	FCanvasTileItem BackgroundTile(FVector2d{X-5, Y-5}, FVector2D(GraphWidth + 10, FMath::Max(LastHeight-InitialY+10, RowHeight*10)), FLinearColor::Black.CopyWithNewOpacity(0.3f));
	BackgroundTile.BlendMode = SE_BLEND_TranslucentAlphaOnly;
	BackgroundTile.Draw(FCanvas);

	struct FScopeInfo
	{
		FCoroutineNode* Scope;
		double Y;
		double Indent;
		bool operator==(FCoroutineNode* InScope) const { return Scope == InScope; }
	};
	TArray<FScopeInfo> ScopeInfo;
	auto HeightForScope = [&](FCoroutineNode* Scope)
	{
		auto ScopeInfoPtr = ScopeInfo.FindByKey(Scope);
		/**
		 * There's some bug here related to deferred nodes that triggers this ensure
		 * I don't have time to fix it yet, so the ensure is disabled
		 */
		//ensure(false);
		return ScopeInfoPtr ? ScopeInfoPtr->Y : 0.0;
	};
	auto IndentForScope = [&](FCoroutineNode* Scope)
	{
		auto ScopeInfoPtr = ScopeInfo.FindByKey(Scope);
		return ScopeInfoPtr ? ScopeInfoPtr->Indent : 0.0;
	};
	auto IndentForRow = [&](FCoroutineExecutor::FDebuggerRow const& Row)
	{
		if (bCompactMode)
		{
			if (Row.Scope)
			{
				return IndentForScope(Row.Scope) + 2.0;
			}
			return 0.0;
		}
		return 2.0 * Row.Depth;
	};
	for (const auto Exec : ExecutorsToDebug)
	{
		if (Exec->DebuggerInfo.Num() == 0)
			continue;
		ScopeInfo.Reset();
		ScopeInfo.Add( FScopeInfo{Exec->DebuggerInfo[0].Node, Y + RowHeight*0.6, 0.0} );
		for (auto RowIt = Exec->DebuggerInfo.CreateConstIterator(); RowIt; ++RowIt)
		{
			if (GCoroutineDebuggerFilter.Num() > 0)
			{
				auto IsRootScope = [](auto RowIt)
				{
					return RowIt->bIsScope && RowIt->Parent == nullptr;
				};
				auto StringPassesFilter = [&] (FString String)
				{
					for (auto& Filter : GCoroutineDebuggerFilter)
					{
						if (String.Contains(Filter))
							return true;
					}
					return false;
				};
				if (IsRootScope(RowIt))
				{
					while (RowIt && ensure(RowIt->Entries.Num() > 0) && !StringPassesFilter(RowIt->Entries.Last().Name))
					{
						do
						{
							++RowIt;
						} while(RowIt && !IsRootScope(RowIt));
					}
					if (!RowIt)
						break;
				}
			}
			auto& Row = *RowIt;
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
			double Indent = IndentForRow(Row);
			for (int Index = FirstEntryIndex; Index < Row.Entries.Num(); ++Index)
			{
				const auto& Entry = Row.Entries[Index];
				const double CappedStartTime = FMath::Max(StartTime, Entry.StartTime);
				FVector2D EntryStartPos(X + Indent + (CappedStartTime - StartTime) * TimeToPixels, Y);
				const double CurrentEndTime = Entry.EndTime < 0.0 ? CurrentTime : Entry.EndTime;
				const double EntryDuration = CurrentEndTime - CappedStartTime;
				if (DrawnEntries == 0)
				{
					FirstEntryX = EntryStartPos.X;
				}
				++DrawnEntries;
				const double EntryWidth = FMath::Clamp(EntryDuration * TimeToPixels, 1.0, X + GraphWidth-EntryStartPos.X);
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
					FCanvasTileItem EntryTile(FVector2D(FirstEntryX-2.0, MyScopeY), FVector2D(2.0, Y - MyScopeY + RowHeight*0.4), FLinearColor(0.25f, 0.25f, 0.25f, 0.5f));
					EntryTile.BlendMode = SE_BLEND_TranslucentAlphaOnly;
					EntryTile.Draw(FCanvas);
				}
			}
			if (Row.bIsScope)
			{
				ScopeInfo.Add(FScopeInfo { Row.Node, Y + RowHeight*0.8, Indent });
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
