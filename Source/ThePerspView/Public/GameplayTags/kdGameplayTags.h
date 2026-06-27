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
	FGameplayTag Ability_ShadowDash;
	FGameplayTag Ability_StrategicView;

	//Attributes
	FGameplayTag Attribute_ShadowStamina;
	FGameplayTag Attribute_MaxShadowStamina;
	FGameplayTag Attribute_LightHealth;
	FGameplayTag Attribute_MaxLightHealth;

	//States
	FGameplayTag State_CrushMode;
	FGameplayTag State_Transitioning;
	FGameplayTag State_InShadow;
	FGameplayTag State_Exhausted;
	FGameplayTag State_Dashing;
	FGameplayTag State_EnemyContact;
	FGameplayTag State_InLight;

	//Effects
	FGameplayTag Effect_ShadowDrain;
	FGameplayTag Effect_LightDamage;  
	FGameplayTag Effect_ShadowHeal;

	//Interactions
	FGameplayTag Interact_CrushOnly;

	// Datas
	FGameplayTag Data_StaminaDelta;


private:
	static FkdGameplayTags GameplayTags;
};
