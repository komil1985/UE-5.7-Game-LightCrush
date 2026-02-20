// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "kdAssetManager.generated.h"

/**
 * 
 */
UCLASS()
class THEPERSPVIEW_API UkdAssetManager : public UAssetManager
{
	GENERATED_BODY()
	
public:
	static UkdAssetManager& Get();

protected:
	virtual void StartInitialLoading() override;
};
