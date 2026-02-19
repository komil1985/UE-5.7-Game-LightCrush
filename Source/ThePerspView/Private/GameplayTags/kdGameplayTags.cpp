// Copyright ASKD Games


#include "GameplayTags/kdGameplayTags.h"
#include "GameplayTagsManager.h"

FkdGameplayTags FkdGameplayTags::GameplayTags;

void FkdGameplayTags::InitializeNativeGameplayTags()
{
	UGameplayTagsManager::Get().AddNativeGameplayTag(FName("Abilities.Main.LightCrush"), FString("Light Crush Ability"));
}
