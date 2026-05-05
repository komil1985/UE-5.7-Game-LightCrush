// Copyright ASKD Games


#include "AbilitySystem/kdAttributeSet.h"
#include "GameplayTags/kdGameplayTags.h"
#include "GameplayEffectExtension.h"


UkdAttributeSet::UkdAttributeSet()
{
    InitShadowStamina(100.f);      
    InitMaxShadowStamina(100.f);   

    InitLightHealth(100.f);        
    InitMaxLightHealth(100.f);
}


void UkdAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    if (Attribute == GetShadowStaminaAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxShadowStamina());
    }
    else if (Attribute == GetLightHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxLightHealth());
    }
}

void UkdAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    if (Data.EvaluatedData.Attribute == GetShadowStaminaAttribute())
    {
        float NewStamina = FMath::Clamp(ShadowStamina.GetCurrentValue(), 0.0f, MaxShadowStamina.GetCurrentValue());
        ShadowStamina.SetCurrentValue(NewStamina);

#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log, TEXT("Stamina changed to: %f"), NewStamina);
#endif

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
                if (ASC->HasMatchingGameplayTag(Tags.State_InShadow))
                {
                    ASC->RemoveLooseGameplayTag(Tags.State_InShadow);
                }
                // Cancel abilities with the crush tag
                FGameplayTagContainer CrushAbilityTag;
                CrushAbilityTag.AddTag(Tags.Ability_LightCrush); // assuming this tag is on the crush ability
                ASC->CancelAbilities(&CrushAbilityTag);

                // Remove the drain effect by its granted tag
                FGameplayTagContainer EffectTag;
                EffectTag.AddTag(Tags.Effect_ShadowDrain);
                ASC->RemoveActiveEffectsWithGrantedTags(EffectTag);
            }
        }
        else
        {
            // Stamina recovered — clear exhausted so abilities re-enable
            if (ASC->HasMatchingGameplayTag(Tags.State_Exhausted))
            {
                ASC->RemoveLooseGameplayTag(Tags.State_Exhausted);
            }
        }
    }
    else if (Data.EvaluatedData.Attribute == GetLightHealthAttribute())
    {
        float NewHealth = FMath::Clamp(LightHealth.GetCurrentValue(), 0.f, MaxLightHealth.GetCurrentValue());
        LightHealth.SetCurrentValue(NewHealth);

#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log, TEXT("Health changed to: %f"), NewHealth);
#endif
    }
}
