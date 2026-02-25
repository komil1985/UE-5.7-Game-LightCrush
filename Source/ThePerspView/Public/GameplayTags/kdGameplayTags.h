// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * kdGameplayTags
 * 
 * Singleton containing native gameplay tags
 */

struct FkdGameplayTags
{
public:
	static const FkdGameplayTags& Get() { return GameplayTags; }

	static void InitializeNativeGameplayTags();

	// Abilities
	FGameplayTag Ability_LightCrush;
	FGameplayTag Ability_ShadowJump;
	FGameplayTag Ability_Block_Crush;

	//Attributes
	FGameplayTag Attribute_ShadowStamina;
	FGameplayTag Attribute_MaxShadowStamina;

	//States
	FGameplayTag State_CrushMode;
	FGameplayTag State_Transitioning;
	FGameplayTag State_InShadow;
	FGameplayTag State_Exhausted;

	//Interactions
	FGameplayTag Interact_CrushOnly;

private:
	static FkdGameplayTags GameplayTags;
};
