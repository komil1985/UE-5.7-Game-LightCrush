// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/PostProcessVolume.h"
#include "kdWorldColorDriver.generated.h"

class UkdColorTheme;
class UPostProcessComponent;
class UMaterialParameterCollectionInstance;



UCLASS(ClassGroup = "ASKD", meta = (BlueprintSpawnableComponent))
class THEPERSPVIEW_API UkdWorldColorDriver : public UActorComponent
{
    GENERATED_BODY()

public:
    UkdWorldColorDriver();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // ── Configuration ────────────────────────────────────────────────────────

    /** Assign DA_LiminalDusk here in the player Blueprint. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color Driver")
    TObjectPtr<UkdColorTheme> ColorTheme;

    // ── Blueprint Hooks ───────────────────────────────────────────────────────

    /** Fires at the first frame a blend begins (before any alpha change). */
    UFUNCTION(BlueprintImplementableEvent, Category = "Color Driver")
    void BP_OnBlendStarted(bool bEnteringCrushMode);

    /** Fires once the blend has settled at 0 (light) or 1 (shadow). */
    UFUNCTION(BlueprintImplementableEvent, Category = "Color Driver")
    void BP_OnBlendFinished(bool bInCrushMode);

    // ── Runtime Queries ───────────────────────────────────────────────────────

    /** True while a post-process blend is in progress. */
    UFUNCTION(BlueprintPure, Category = "Color Driver")
    bool IsBlending() const { return bBlending; }

    /**
     * Current blend alpha.
     * 0.0 = fully in the light world.
     * 1.0 = fully in the shadow world.
     * Values between 0 and 1 mean a blend is in progress.
     * Wire to UkdHUDColorLibrary::GetTransitionFlashColor for screen-edge VFX.
     */
    UFUNCTION(BlueprintPure, Category = "Color Driver")
    float GetBlendAlpha() const { return BlendAlpha; }

private:
    UPROPERTY()
    TObjectPtr<APostProcessVolume> CachedShadowVolume;

    UPROPERTY()
    TObjectPtr<APostProcessVolume> CachedLightVolume;
    // ── Tag Callback (same registration pattern as AkdShadowPortal) ──────────

    UFUNCTION()
    void OnCrushModeTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

    // ── Blend Internals ───────────────────────────────────────────────────────

    void StartBlend(bool bEnteringCrushMode);
    void ApplyProfileToPostProcess(float Alpha) const;
    void UpdateMPC(float Alpha) const;

    float BlendAlpha = 0.f;     // 0 = light world, 1 = shadow world
    float BlendDirection = 0.f;     // +1 towards shadow | -1 towards light
    bool  bBlending = false;

    // ── Cached References ─────────────────────────────────────────────────────

    UPROPERTY()
    TObjectPtr<UPostProcessComponent> PostProcessComp;

    UPROPERTY()
    TObjectPtr<UMaterialParameterCollectionInstance> MPCInstance;

    // ── MPC Parameter Name Constants ──────────────────────────────────────────
    // Must match the exact names you use inside your MPC asset.

    static const FName ParamName_WorldBlendAlpha;
    static const FName ParamName_CrushModeColor;
    static const FName ParamName_LightWorldColor;
};
