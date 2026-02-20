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
	FGameplayTag Abilities_LightCrush;
	FGameplayTag Abilities_ShadowJump;

	//Attributes
	FGameplayTag Attributes_ShadowStamina;
	FGameplayTag Attributes_MaxShadowStamina;

private:
	static FkdGameplayTags GameplayTags;
};
