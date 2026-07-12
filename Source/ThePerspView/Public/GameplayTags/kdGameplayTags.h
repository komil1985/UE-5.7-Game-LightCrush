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
	FGameplayTag State_Dead;

	//Effects
	FGameplayTag Effect_ShadowDrain;
	FGameplayTag Effect_LightDamage;  
	FGameplayTag Effect_ShadowHeal;

	//Interactions
	FGameplayTag Interact_CrushOnly;

	// Tutorial action locks — present on ASC ⇒ that action is blocked.
	// Only applied inside the tutorial; absent everywhere else.
	FGameplayTag Tutorial_Lock_Move;
	FGameplayTag Tutorial_Lock_Look;
	FGameplayTag Tutorial_Lock_Jump;
	FGameplayTag Tutorial_Lock_Crush;
	FGameplayTag Tutorial_Lock_Dash;
	FGameplayTag Tutorial_Lock_StrategicView;
	FGameplayTag Tutorial_Lock_Interact;

	// Datas
	FGameplayTag Data_StaminaDelta;


private:
	static FkdGameplayTags GameplayTags;
};
