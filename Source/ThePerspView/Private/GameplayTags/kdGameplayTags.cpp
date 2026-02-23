// Copyright ASKD Games


#include "GameplayTags/kdGameplayTags.h"
#include "GameplayTagsManager.h"

FkdGameplayTags FkdGameplayTags::GameplayTags;

void FkdGameplayTags::InitializeNativeGameplayTags()
{
	// Attributes Tags
	GameplayTags.Attributes_ShadowStamina = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.ShadowStamina"),
		FString("Stamina used while moving in shadow")
	);

	GameplayTags.Attributes_MaxShadowStamina = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.MaxShadowStamina"),
		FString("Maximum Stamina")
	);


	// Abilities Tags
	GameplayTags.Abilities_LightCrush = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.LightCrush"),
		FString("Ability to crush light")
	);

	GameplayTags.Abilities_ShadowJump = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.ShadowJump"),
		FString("Ability to jump while in shadow")
	);

	// States Tags
	GameplayTags.State_CrushMode = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.CrushMode"),
		FString("Crush Mode State")
	);

	GameplayTags.State_InShadow = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.InShadow"),
		FString("In Shadow State")
	);

	GameplayTags.State_Exhausted = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.Exhausted"),
		FString("Exhausted State")
	);

	// Interaction Tags
	GameplayTags.Interact_CrushOnly = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Interact.CrushOnly"),
		FString("Interact in Crush Only")
	);
}
