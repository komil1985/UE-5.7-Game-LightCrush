// Copyright ASKD Games


#include "AbilitySystem/Effects/kdShadowDrain.h"
#include "GameplayTags/kdGameplayTags.h"
#include "AbilitySystem/kdAttributeSet.h"

UkdShadowDrain::UkdShadowDrain()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	Period = 0.2f;		// drain every 0.2 seconds

    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UkdAttributeSet::GetShadowStaminaAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FScalableFloat(-5.f);

    Modifiers.Add(Modifier);

    const FkdGameplayTags& Tags = FkdGameplayTags::Get();

    // Only active while crushing AND in shadow
    FGameplayTagContainer RequiredTags;
    RequiredTags.AddTag(Tags.State_CrushMode);
    RequiredTags.AddTag(Tags.State_InShadow);

    OngoingTagRequirements.RequireTags = RequiredTags;
}
