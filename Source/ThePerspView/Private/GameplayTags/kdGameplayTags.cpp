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
}
