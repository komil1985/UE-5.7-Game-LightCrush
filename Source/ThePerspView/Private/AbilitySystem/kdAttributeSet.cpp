// Copyright ASKD Games


#include "AbilitySystem/kdAttributeSet.h"
#include "GameplayTags/kdGameplayTags.h"
#include "GameplayEffectExtension.h"

UkdAttributeSet::UkdAttributeSet()
{
}

void UkdAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	// Handle Shadow Stamina clamping and exhaustion state 
    if (Data.EvaluatedData.Attribute == GetShadowStaminaAttribute())
    {
        ShadowStamina.SetCurrentValue(
            FMath::Clamp(ShadowStamina.GetCurrentValue(), 0.f, MaxShadowStamina.GetCurrentValue())
        );

        if (ShadowStamina.GetCurrentValue() <= 0.f)
        {
            if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
            {
                ASC->AddLooseGameplayTag(FkdGameplayTags::Get().State_Exhausted);
                ASC->RemoveLooseGameplayTag(FkdGameplayTags::Get().State_CrushMode);
            }
        }
    }
}
