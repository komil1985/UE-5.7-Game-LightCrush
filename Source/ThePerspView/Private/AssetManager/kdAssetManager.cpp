// Copyright ASKD Games


#include "AssetManager/kdAssetManager.h"
#include "GameplayTags/kdGameplayTags.h"


UkdAssetManager& UkdAssetManager::Get()
{
	check(GEngine);

	UkdAssetManager* AssetManager = Cast<UkdAssetManager>(GEngine->AssetManager);
	return *AssetManager;
}

void UkdAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	FkdGameplayTags::InitializeNativeGameplayTags();

}
