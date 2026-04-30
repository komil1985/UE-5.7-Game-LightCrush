// Copyright ASKD Games


#include "UI/Widget/kdLowStaminaWidget.h"
#include "Components/TextBlock.h"


void UkdLowStaminaWidget::FlashWarning()
{
    if (Txt_LowStamina)
    {
        Txt_LowStamina->SetText(FText::FromString(TEXT("LOW STAMINA!")));
        // Vivid red — readable at a glance, consistent with the exhaustion palette
        Txt_LowStamina->SetColorAndOpacity(FLinearColor(0.65f, 0.05f, 0.05f, 1.f));
    }

    // Reveal widget — HitTestInvisible so it never accidentally absorbs input
    SetVisibility(ESlateVisibility::HitTestInvisible);

    // Fire Blueprint animation hook every press (scale-punch, shake, etc.)
    BP_OnFlash();

    // Re-arm the auto-hide timer; calling SetTimer on an already-running handle
    // automatically cancels and restarts it — exactly what we want on rapid presses.
    UWorld* World = GetWorld();
    if (ensureMsgf(World, TEXT("UkdLowStaminaWidget::FlashWarning — no World")))
    {
        World->GetTimerManager().SetTimer(HideTimerHandle, this, &UkdLowStaminaWidget::HideWarning, FlashDuration, false);  // non-looping
    }
}

void UkdLowStaminaWidget::HideWarning()
{
    SetVisibility(ESlateVisibility::Hidden);
}
