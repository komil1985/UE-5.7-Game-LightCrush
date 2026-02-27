// Copyright ASKD Games


#include "AbilitySystem/kdAttributeSet.h"
#include "GameplayTags/kdGameplayTags.h"
#include "GameplayEffectExtension.h"

UkdAttributeSet::UkdAttributeSet()
{
}

void UkdAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    if (Data.EvaluatedData.Attribute == GetShadowStaminaAttribute())
    {
        float NewStamina = FMath::Clamp(ShadowStamina.GetCurrentValue(), 0.f, MaxShadowStamina.GetCurrentValue());
        ShadowStamina.SetCurrentValue(NewStamina);

        UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
        if (!ASC) return;

        const FkdGameplayTags& Tags = FkdGameplayTags::Get();

        if (NewStamina <= 0.f)
        {
            ASC->AddLooseGameplayTag(Tags.State_Exhausted);
            ASC->RemoveLooseGameplayTag(Tags.State_CrushMode);
        }
        else
        {
            ASC->RemoveLooseGameplayTag(Tags.State_Exhausted);
        }
    }
}

void UkdAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    if (Attribute == GetShadowStaminaAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxShadowStamina());
    }
}
