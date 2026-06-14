// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Engine/PostProcessVolume.h"
#include "AkdGlobalPostProcessVolume.generated.h"

/**
 * 
 */
UCLASS()
class THEPERSPVIEW_API AAkdGlobalPostProcessVolume : public APostProcessVolume
{
	GENERATED_BODY()

public:
	AAkdGlobalPostProcessVolume();

	/**
	 * Guarantees exactly one AkdGlobalPostProcessVolume exists in World.
	 * No-op if one is already present. Cheap - safe to call every level
	 * load.
	 */
	static AAkdGlobalPostProcessVolume* EnsureExists(UWorld* World);

protected:
	// Soft refs: materials load only when this volume is constructed,
	// never at module/class load time.
	UPROPERTY(EditDefaultsOnly, Category = "kd|PostProcess")
	TSoftObjectPtr<UMaterialInterface> CrushShadowStencilMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "kd|PostProcess")
	TSoftObjectPtr<UMaterialInterface> NeonPreservationMaterial;

private:
	void ConfigureBlendables();
	void ConfigureBaselineSettings();
	
};
