// Copyright ASKD Games


#include "AbilitySystem/kdAttributeSet.h"
#include "GameplayTags/kdGameplayTags.h"
#include "GameplayEffectExtension.h"


UkdAttributeSet::UkdAttributeSet(){}


void UkdAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    if (Attribute == GetShadowStaminaAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxShadowStamina());
    }
}

void UkdAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    if (Data.EvaluatedData.Attribute == GetShadowStaminaAttribute())
    {
        float NewStamina = FMath::Clamp(ShadowStamina.GetCurrentValue(), 0.f, MaxShadowStamina.GetCurrentValue());
        ShadowStamina.SetCurrentValue(NewStamina);

        UE_LOG(LogTemp, Log, TEXT("Stamina changed to: %f"), NewStamina);

        UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
        if (!ASC) return;

        const FkdGameplayTags& Tags = FkdGameplayTags::Get();

        if (NewStamina <= 0.f)
        {
            if (!ASC->HasMatchingGameplayTag(Tags.State_Exhausted))
            {
                ASC->AddLooseGameplayTag(Tags.State_Exhausted);
            }
            
            // Remove crush mode tag and cancel any active crush ability
            if (ASC->HasMatchingGameplayTag(Tags.State_CrushMode))
            {
                ASC->RemoveLooseGameplayTag(Tags.State_CrushMode);
                // Cancel abilities with the crush tag
                FGameplayTagContainer CrushAbilityTag;
                CrushAbilityTag.AddTag(Tags.Ability_LightCrush); // assuming this tag is on the crush ability
                ASC->CancelAbilities(&CrushAbilityTag);
            }
        }
        else
        {
            if (ASC->HasMatchingGameplayTag(Tags.State_Exhausted))
            {
                ASC->RemoveLooseGameplayTag(Tags.State_Exhausted);
            }
        }
    }
}
