// Copyright ASKD Games


#include "UI/Widget/KdLevelSelectWidget.h"
#include "UI/Widget/KdLevelSelectButtonWidget.h"
#include "GameInstance/kdGameInstance.h"
#include "Components/PanelWidget.h"
#include "Components/Button.h"
#include "Components/UniformGridSlot.h"
#include "Components/GridSlot.h"
#include "UI/HUD/kdHUD.h"



void UKdLevelSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (BackButton)
	{
		BackButton->OnClicked.AddDynamic(this, &UKdLevelSelectWidget::HandleBackClicked);
	}
}

void UKdLevelSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshLevelButtons();
}

void UKdLevelSelectWidget::RefreshLevelButtons()
{
	if (!LevelButtonContainer || !LevelButtonClass)
	{
		return;
	}

	LevelButtonContainer->ClearChildren();

	UkdGameInstance* GI = UkdGameInstance::Get(this);
	if (!GI)
	{
		return;
	}

	const FName MenuName = GI->GetMainMenuLevelName();
	const int32 Total = GI->GetTotalLevelCount();
	const int32 Columns = FMath::Max(1, ColumnsPerRow);

	// Walk LevelOrder directly so this list can never drift out of sync with it.
	// The main-menu entry (index 0 in the default setup) is skipped.
	int32 DisplayNumber = 0;
	for (int32 Index = 0; Index < Total; ++Index)
	{
		const FName LevelName = GI->GetLevelNameAtIndex(Index);
		if (LevelName.IsNone() || LevelName == MenuName)
		{
			continue;
		}

		++DisplayNumber;

		UKdLevelSelectButtonWidget* Entry = CreateWidget<UKdLevelSelectButtonWidget>(this, LevelButtonClass);
		if (!Entry)
		{
			continue;
		}

		const bool bUnlocked = GI->IsLevelUnlocked(Index);

		// A best time only gets recorded on completion (RecordLevelComplete -> TrySetBestTime),
		// so >0 is a reliable "cleared at least once" signal. Swap this for a dedicated
		// completed flag if you add one to UkdSaveGame later.
		const bool bCompleted = GI->GetBestTime(Index) > 0.f;

		FText Label;
		if (const FText* Override = DisplayNameOverrides.Find(LevelName))
		{
			Label = *Override;
		}
		else
		{
			Label = FText::Format(
				NSLOCTEXT("LevelSelect", "LevelNumber", "Level {0}"),
				FText::AsNumber(DisplayNumber));
		}

		Entry->Setup(Index, Label, bUnlocked, bCompleted);
		Entry->OnClicked.AddDynamic(this, &UKdLevelSelectWidget::HandleLevelButtonClicked);

		UPanelSlot* NewSlot = LevelButtonContainer->AddChild(Entry);

		// Grid panels drop every child into cell (0,0) unless you assign a row/column, so
		// the buttons stack on top of each other and only the last-added one receives clicks
		// (the "only the last level is selectable" symptom). Giving each its own cell fixes
		// it. Box containers (Vertical/Horizontal/Wrap) flow their children themselves, so
		// these casts just no-op there and layout is unaffected.
		const int32 SlotIndex = DisplayNumber - 1;
		const int32 Row = SlotIndex / Columns;
		const int32 Column = SlotIndex % Columns;

		if (UUniformGridSlot* UniformSlot = Cast<UUniformGridSlot>(NewSlot))
		{
			UniformSlot->SetRow(Row);
			UniformSlot->SetColumn(Column);
		}
		else if (UGridSlot* RegularGridSlot = Cast<UGridSlot>(NewSlot))
		{
			RegularGridSlot->SetRow(Row);
			RegularGridSlot->SetColumn(Column);
		}
	}
}

void UKdLevelSelectWidget::HandleLevelButtonClicked(int32 LevelOrderIndex)
{
	UkdGameInstance* GI = UkdGameInstance::Get(this);
	if (!GI)
	{
		return;
	}

	if (!GI->IsLevelUnlocked(LevelOrderIndex))
	{
		return;
	}

	const FName LevelName = GI->GetLevelNameAtIndex(LevelOrderIndex);
	if (LevelName.IsNone())
	{
		return;
	}

	// Show the loading screen first, then load after a short delay — same pattern
	// the main menu's New Game / Continue buttons use.
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AkdHUD* HUD = Cast<AkdHUD>(PC->GetHUD()))
		{
			HUD->ShowLoadingScreen();
		}
	}

	constexpr float LoadDelay = 3.0f;
	FTimerHandle LoadHandle;
	//GetWorld()->GetTimerManager().SetTimerForNextTick([]() {}); // no-op to ensure timer manager is valid
	GetWorld()->GetTimerManager().SetTimer(LoadHandle, [GI, LevelName]()
		{
			GI->LoadLevel(LevelName);
		}, LoadDelay, false);
}

void UKdLevelSelectWidget::HandleBackClicked()
{
	// The HUD hid the main menu when it showed us, so let it collapse this screen
	// and restore the menu — same round-trip as Settings' Back button.
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AkdHUD* HUD = Cast<AkdHUD>(PC->GetHUD()))
		{
			HUD->HideLevelSelect();
			return;
		}
	}

	// Fallback if this widget is ever shown without the HUD driving it.
	RemoveFromParent();
}

