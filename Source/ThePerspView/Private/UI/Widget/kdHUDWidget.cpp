// Copyright ASKD Games


#include "UI/Widget/kdHUDWidget.h"
#include "Components/ProgressBar.h"

void UkdHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Apply whatever the presenter pushed before our sub-widgets were bound.
    SetHealthPercent(PendingHealth01);
    SetStaminaPercent(PendingStamina01);
    SetExhausted(bPendingExhausted);
}

void UkdHUDWidget::SetHealthPercent(float InPercent01)
{
    PendingHealth01 = FMath::Clamp(InPercent01, 0.f, 1.f);
    if (IsValid(Bar_Health))
    {
        Bar_Health->SetPercent(PendingHealth01);
    }
}

void UkdHUDWidget::SetStaminaPercent(float InPercent01)
{
    PendingStamina01 = FMath::Clamp(InPercent01, 0.f, 1.f);
    if (IsValid(Bar_Stamina))
    {
        Bar_Stamina->SetPercent(PendingStamina01);
    }
}

void UkdHUDWidget::SetExhausted(bool bExhausted)
{
    bPendingExhausted = bExhausted;
    // View-only flourish. Left minimal; extend here or in the WBP with theme colors.
}

