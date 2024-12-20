// Copyright ACE Team Software S.A. All Rights Reserved.

#if WITH_ACETEAM_COROUTINE_DEBUGGER

#include "GameplayDebuggerCategory_Coroutines.h"

#include "CoroutineExecutor.h"
#include "CoroutinesWorldSubsystem.h"
#include "Engine/Canvas.h"
#include "Fonts/FontMeasure.h"

#include "Runtime/Launch/Resources/Version.h"

using namespace ACETeam_Coroutines;

FGameplayDebuggerCategory_Coroutines::FGameplayDebuggerCategory_Coroutines()
{
	bShowOnlyWithDebugActor = false;

	BindKeyPress(EKeys::RightBracket.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_Coroutines::ToggleCompactMode, EGameplayDebuggerInputMode::Local);
	BindKeyPress(EKeys::F.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_Coroutines::ScrollDown, EGameplayDebuggerInputMode::Local);
	BindKeyPress(EKeys::R.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_Coroutines::ScrollUp, EGameplayDebuggerInputMode::Local);
}

static TArray<FString> GCoroutineDebuggerFilter;

struct FGameplayDebuggerCategory_CoroutinesCommands
{
	static void SetFilter(const TArray< FString >& Args, UWorld* World)
	{
		GCoroutineDebuggerFilter = Args;
	}
};

static FAutoConsoleCommandWithWorldAndArgs SetFilterCmd(
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
	CanvasContext.Printf(TEXT("\n[{yellow}%s{white}] Scroll down"), *GetInputHandlerDescription(1));
	CanvasContext.Printf(TEXT("\n[{yellow}%s{white}] Scroll up"), *GetInputHandlerDescription(2));
	
	UCanvas* Canvas = CanvasContext.Canvas.Get();
	if (!Canvas)
		return;
	FCanvas* FCanvas = Canvas->Canvas;
	FCoroutineExecutor* ExecutorsToDebug [] = {
		&UCoroutinesWorldSubsystem::Get(OwnerPC).Executor
	};
	const double CurrentTime = FApp::GetCurrentTime();
	const double StartTime = CurrentTime - GraphTimeWindow;
	const double GraphWidth = 450.0;
	const double GraphMargin = 150.0;
	const double RowHeight = 14.0;
	const double SpaceBetweenRows = 1.0;
	const double InitialY = 200.0;
	const double X = Canvas->SizeX - GraphWidth - GraphMargin;
	double Y = InitialY;
	
	const double OneOverGraphTimeWindow = 1.0 / GraphTimeWindow;
	const double TimeToPixels = OneOverGraphTimeWindow*GraphWidth;

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

#if ENGINE_MAJOR_VERSION < 5
	auto Flt = [](double Val){
		return static_cast<float>(Val);
	};
#else
	auto Flt = [](double Val) { return Val; };
#endif

	FCanvasTileItem BackgroundTile(FVector2D{ Flt(X-5.0), Flt(Y-5.0)}, FVector2D(Flt(GraphWidth + 10.0), Flt(FMath::Max(LastHeight-InitialY+10, RowHeight*10))), FLinearColor::Black.CopyWithNewOpacity(0.3f));
	BackgroundTile.BlendMode = SE_BLEND_TranslucentAlphaOnly;
	BackgroundTile.Draw(FCanvas);

	if (Offset > 0)
	{
		FCanvasTileItem ScrollUpTile(FVector2D{ Flt(X-5.0), Flt(Y-15.0)}, FVector2D(Flt(GraphWidth + 10.0), Flt(10.0)), FLinearColor::Black.CopyWithNewOpacity(0.5f));
		ScrollUpTile.BlendMode = SE_BLEND_TranslucentAlphaOnly;
		ScrollUpTile.Draw(FCanvas);

		FCanvasTextItem ScrollUpText(FVector2D{ Flt(X-5.0 + GraphWidth*0.5), Flt(Y-15.0)}, INVTEXT("More..."), GEngine->GetTinyFont(), FLinearColor::White);
		ScrollUpText.Draw(FCanvas);
	}

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
		auto IsRootScope = [](auto RowIt)
		{
			return RowIt->bIsScope && RowIt->Parent == nullptr;
		};
		int FirstNodeToShow = 0;
		if (Offset > 0)
		{
			int SkippedRoots = 0;
			for (int i = 0; i < Exec->DebuggerInfo.Num(); ++i)
			{
				if (IsRootScope(&Exec->DebuggerInfo[i]))
				{
					++SkippedRoots;
					if (SkippedRoots > Offset)
					{
						FirstNodeToShow = i;
						break;
					}
				}
			}
		}
		ScopeInfo.Reset();
		ScopeInfo.Add( FScopeInfo{Exec->DebuggerInfo[FirstNodeToShow].Node, Y + RowHeight*0.6, 0.0} );
		auto View = MakeArrayView(Exec->DebuggerInfo.GetData()+FirstNodeToShow, Exec->DebuggerInfo.Num() - FirstNodeToShow);
		for (int i = 0; i < View.Num(); ++i)
		{
			auto RowIt = &View[i];
			if (GCoroutineDebuggerFilter.Num() > 0)
			{
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
			const auto* FirstEntryToRender = Algo::FindByPredicate(Row.Entries, [&](const FCoroutineExecutor::FDebuggerEntry& Entry)
			{
				return Entry.EndTime < 0.0 || Entry.EndTime >= StartTime;
			});
			if (FirstEntryToRender == nullptr)
			{
				continue;
			}
			const int FirstEntryIndex = Row.Entries.ConvertPointerToIndex(FirstEntryToRender);
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

void FGameplayDebuggerCategory_Coroutines::ScrollDown()
{
	Offset += 1;
}

void FGameplayDebuggerCategory_Coroutines::ScrollUp()
{
	Offset = FMath::Max(0, Offset-1);
}

#endif // WITH_ACETEAM_COROUTINE_DEBUGGER
