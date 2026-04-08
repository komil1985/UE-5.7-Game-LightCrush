// Copyright ASKD Games


#include "AbilitySystem/Effects/kdShadowDashCooldown.h"
#include "GameplayTags/kdGameplayTags.h"
#include "AbilitySystem/kdAttributeSet.h"

UkdShadowDashCooldown::UkdShadowDashCooldown()
{

	// Fixed duration — expires after 0.8 seconds, releasing the block on dash
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.8f));

	// While this effect is active, the ASC owns State.Dashing.
	// UkdShadowDash::ActivationBlockedTags contains State.Dashing, so the
	// ability's CanActivate check automatically fails until this expires.
	//
	// GrantedTags.Added is populated here because FkdGameplayTags is
	// initialized by UkdAssetManager::StartInitialLoading before any
	// gameplay objects (and their CDOs) are ever accessed.
	const FkdGameplayTags& Tags = FkdGameplayTags::Get();
	InheritableOwnedTagsContainer.AddTag(Tags.State_Dashing);
}
