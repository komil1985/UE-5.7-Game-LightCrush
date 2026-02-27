// Copyright ASKD Games


#include "GameplayTags/kdGameplayTags.h"
#include "GameplayTagsManager.h"

FkdGameplayTags FkdGameplayTags::GameplayTags;

void FkdGameplayTags::InitializeNativeGameplayTags()
{
	UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("Initializing native gameplay tags..."));
#endif

	// Attributes Tags
	GameplayTags.Attribute_ShadowStamina = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attribute.ShadowStamina"),
		FString("Stamina used while moving in shadow")
	);
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("Created tag: %s"), *GameplayTags.Attribute_ShadowStamina.ToString());
#endif

	GameplayTags.Attribute_MaxShadowStamina = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attribute.MaxShadowStamina"),
		FString("Maximum Stamina")
	);
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("Created tag: %s"), *GameplayTags.Attribute_MaxShadowStamina.ToString());
#endif


	// Abilities Tags
	GameplayTags.Ability_LightCrush = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Ability.LightCrush"),
		FString("Ability to crush light")
	);
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("Created tag: %s"), *GameplayTags.Ability_LightCrush.ToString());
#endif

	GameplayTags.Ability_ShadowJump = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Ability.ShadowJump"),
		FString("Ability to jump while in shadow")
	);
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("Created tag: %s"), *GameplayTags.Ability_ShadowJump.ToString());
#endif

	GameplayTags.Ability_Block_Crush = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Ability.Block.Crush"),
		FString("Ability to block crush mode")
	);
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("Created tag: %s"), *GameplayTags.Ability_Block_Crush.ToString());
#endif

	// States Tags
	GameplayTags.State_CrushMode = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.CrushMode"),
		FString("Player in crush mode")
	);
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("Created tag: %s"), *GameplayTags.State_CrushMode.ToString());
#endif

	GameplayTags.State_Transitioning = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.Transitioning"),
		FString("Player transitioning between states")
	);
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("Created tag: %s"), *GameplayTags.State_Transitioning.ToString());
#endif

	GameplayTags.State_InShadow = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.InShadow"),
		FString("Player standing in shadow")
	);
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("Created tag: %s"), *GameplayTags.State_InShadow.ToString());
#endif

	GameplayTags.State_Exhausted = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.Exhausted"),
		FString("Player stamina depleted")
	);
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("Created tag: %s"), *GameplayTags.State_Exhausted.ToString());
#endif

	// Interaction Tags
	GameplayTags.Interact_CrushOnly = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Interact.CrushOnly"),
		FString("Interact in Crush Only")
	);
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("Created tag: %s"), *GameplayTags.Interact_CrushOnly.ToString());
#endif

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("Gameplay tags initialization complete!"));
#endif
}
