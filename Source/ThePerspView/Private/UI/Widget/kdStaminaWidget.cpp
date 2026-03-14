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

    CurrentStamina = ASC->GetNumericAttribute(UkdAttributeSet::GetShadowStaminaAttribute());
    MaxStamina = ASC->GetNumericAttribute(UkdAttributeSet::GetMaxShadowStaminaAttribute());
}

void UkdStaminaWidget::OnStaminaChanged(const FOnAttributeChangeData& Data)
{
    CurrentStamina = Data.NewValue;

    if (StaminaBar)
    {
        StaminaBar->SetPercent(CurrentStamina / MaxStamina);
    }
}

void UkdStaminaWidget::OnMaxStaminaChanged(const FOnAttributeChangeData& Data)
{
    MaxStamina = Data.NewValue;

    if (StaminaBar)
    {
        StaminaBar->SetPercent(CurrentStamina / MaxStamina);
    }
}
