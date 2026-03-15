// Copyright ASKD Games


#include "UI/Widget/kdStaminaWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/kdAttributeSet.h"
#include "Components/ProgressBar.h"


void UkdStaminaWidget::InitializeWithAbilitySystemComponent(UAbilitySystemComponent* ASC)
{
    AbilitySystemComponent = ASC;

    ASC->GetGameplayAttributeValueChangeDelegate(
        UkdAttributeSet::GetShadowStaminaAttribute()
    ).AddUObject(this, &UkdStaminaWidget::OnStaminaChanged);

    ASC->GetGameplayAttributeValueChangeDelegate(
        UkdAttributeSet::GetMaxShadowStaminaAttribute()
    ).AddUObject(this, &UkdStaminaWidget::OnMaxStaminaChanged);

    SetStaminaBarVisibility(false);
    CurrentStamina = ASC->GetNumericAttribute(UkdAttributeSet::GetShadowStaminaAttribute());
    MaxStamina = ASC->GetNumericAttribute(UkdAttributeSet::GetMaxShadowStaminaAttribute());
    UpdateVisibility();
}

void UkdStaminaWidget::SetStaminaBarVisibility(bool bIsVisible)
{
    // Set visibility on the whole widget or just the progress bar
    SetVisibility(bIsVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

void UkdStaminaWidget::OnStaminaChanged(const FOnAttributeChangeData& Data)
{
    CurrentStamina = Data.NewValue;

    if (StaminaBar)
    {
        StaminaBar->SetPercent(CurrentStamina / MaxStamina);
    }
    UpdateVisibility();
}

void UkdStaminaWidget::OnMaxStaminaChanged(const FOnAttributeChangeData& Data)
{
    MaxStamina = Data.NewValue;

    if (StaminaBar)
    {
        StaminaBar->SetPercent(CurrentStamina / MaxStamina);
    }
    UpdateVisibility();
}

void UkdStaminaWidget::UpdateVisibility()
{
    // Show the bar if current stamina is less than max stamina
    bool bShouldBeVisible = (CurrentStamina < MaxStamina);
    SetVisibility(bShouldBeVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}
