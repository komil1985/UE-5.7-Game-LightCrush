// Copyright ASKD Games


#include "UI/Widget/KdLevelSelectButtonWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/WidgetSwitcher.h"


void UKdLevelSelectButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (SelectButton)
	{
		SelectButton->OnClicked.AddDynamic(this, &UKdLevelSelectButtonWidget::HandleButtonClicked);
	}
}

void UKdLevelSelectButtonWidget::Setup(int32 InLevelIndex, const FText& InDisplayName, bool bUnlocked, bool bCompleted)
{
	LevelIndex = InLevelIndex;

	if (LevelLabelText)
	{
		LevelLabelText->SetText(InDisplayName);
	}

	// This is the actual lock enforcement: a disabled button never fires OnClicked,
	// so a locked level physically cannot be selected from here.
	if (SelectButton)
	{
		SelectButton->SetIsEnabled(bUnlocked);
	}

	if (LockStateSwitcher)
	{
		LockStateSwitcher->SetActiveWidgetIndex(bUnlocked ? 0 : 1);
	}

	if (LockIcon)
	{
		LockIcon->SetVisibility(bUnlocked ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (CompletedCheckmark)
	{
		CompletedCheckmark->SetVisibility(bCompleted ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UKdLevelSelectButtonWidget::HandleButtonClicked()
{
	// Locked entries have their button disabled in Setup(), so this only ever fires
	// for unlocked levels - no extra unlock check needed here.
	OnClicked.Broadcast(LevelIndex);
}

