// Copyright ASKD Games


#include "AbilitySystem/Effects/kdShadowRegen.h"
#include "GameplayTags/kdGameplayTags.h"
#include "AbilitySystem/kdAttributeSet.h"

UkdShadowRegen::UkdShadowRegen()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;

    Period = 0.2f;		// drain every 0.2 seconds

    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UkdAttributeSet::GetShadowStaminaAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FScalableFloat(10.0f); // Regen 10 per second
    Modifiers.Add(Modifier);

    const FkdGameplayTags& Tags = FkdGameplayTags::Get();
    OngoingTagRequirements.RequireTags.AddTag(Tags.State_CrushMode);
    OngoingTagRequirements.RequireTags.AddTag(Tags.State_InShadow);
}
