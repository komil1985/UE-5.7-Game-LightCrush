// Copyright ASKD Games


#include "GameplayTags/kdGameplayTags.h"
#include "GameplayTagsManager.h"

FkdGameplayTags FkdGameplayTags::GameplayTags;

void FkdGameplayTags::InitializeNativeGameplayTags()
{
	// Attributes Tags
	GameplayTags.Attribute_ShadowStamina = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attribute.ShadowStamina"),
		FString("Stamina used while moving in shadow")
	);

	GameplayTags.Attribute_MaxShadowStamina = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attribute.MaxShadowStamina"),
		FString("Maximum Stamina")
	);


	// Abilities Tags
	GameplayTags.Ability_LightCrush = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Ability.LightCrush"),
		FString("Ability to crush light")
	);

	GameplayTags.Ability_ShadowJump = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Ability.ShadowJump"),
		FString("Ability to jump while in shadow")
	);

	GameplayTags.Ability_Block_Crush = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Ability.Block.Crush"),
		FString("Ability to block crush mode")
	);

	// States Tags
	GameplayTags.State_CrushMode = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.CrushMode"),
		FString("Player in crush mode")
	);

	GameplayTags.State_Transitioning = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.Transitioning"),
		FString("Player transitioning between states")
	);

	GameplayTags.State_InShadow = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.InShadow"),
		FString("Player standing in shadow")
	);

	GameplayTags.State_Exhausted = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.Exhausted"),
		FString("Player stamina depleted")
	);

	// Interaction Tags
	GameplayTags.Interact_CrushOnly = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Interact.CrushOnly"),
		FString("Interact in Crush Only")
	);
}
