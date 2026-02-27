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
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("UkdAssetManager::StartInitialLoading - Asset manager initializing"));
#endif

	Super::StartInitialLoading();

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("UkdAssetManager: Initializing gameplay tags..."));
#endif

	FkdGameplayTags::InitializeNativeGameplayTags();

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("UkdAssetManager initialization complete"));
#endif
}
