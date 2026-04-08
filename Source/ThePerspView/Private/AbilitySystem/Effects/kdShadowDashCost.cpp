// Copyright ASKD Games


#include "AbilitySystem/Effects/kdShadowDashCost.h"
#include "AbilitySystem/kdAttributeSet.h"

UkdShadowDashCost::UkdShadowDashCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Subtract 30 stamina per dash. Tune this value in a Blueprint subclass
	// or via a CurveTable if ever want level-scaling later.
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UkdAttributeSet::GetShadowStaminaAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(-30.f);
	Modifiers.Add(Modifier);
}
