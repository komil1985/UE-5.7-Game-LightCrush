// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "kdCrushShadowVolume.generated.h"


/** Which crush axis a shadow volume responds to. */
UENUM(BlueprintType)
enum class EkdCrushAxisFilter : uint8
{
    AnyAxis   UMETA(DisplayName = "Any Axis"),
    XAxisOnly UMETA(DisplayName = "X Only (North / South)"),
    YAxisOnly UMETA(DisplayName = "Y Only (East / West)"),
};


class UStaticMeshComponent;
class UAbilitySystemComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class AkdMyPlayer;

/**
 * AkdCrushShadowVolume
 * ---------------------------------------------------------------------------
 * A crush-mode shadow column.
 *
 * LEGACY BEHAVIOUR (still the default when ShadowVolumeMaterial is unset):
 *   The mesh is a Custom-Depth-ONLY stamp. It is never drawn in the colour
 *   pass, casts nothing, collides with nothing. It only writes ShadowStencilValue
 *   (project convention: 2 = darken) into the custom-depth buffer, and the global
 *   M_CrushPostProcess material reads that stencil to darken the scene. In this
 *   mode there is NO per-volume colour or opacity — the look is global.
 *
 * SELF-RENDERED TINT SLAB (opt-in, when ShadowVolumeMaterial is assigned):
 *   The mesh additionally renders itself as an unlit, translucent "tint slab"
 *   whose colour and opacity are driven per-instance by a UMaterialInstanceDynamic.
 *   This gives true per-volume, editor-tunable ShadowColor + ShadowOpacity that
 *   are entirely self-contained: they live in this actor's own material domain and
 *   never write to MPC_WorldColor or any post-process field, so the single-writer
 *   rule owned by UkdWorldColorDriver is preserved untouched.
 *
 *   INTEGRATION (do once):
 *    - Drop these volumes out of the M_CrushPostProcess stencil-2 darken (or keep
 *      the stencil write purely for pipeline ordering) so the slab is not
 *      double-darkened by both paths.
 *    - The slab material should discard where CustomStencil == 1 (neon-preserve)
 *      so lit / neon elements punch through the shadow.
 *
 *   REQUIRED MATERIAL (M_CrushShadowVolume):
 *    - Shading Model: Unlit
 *    - Blend Mode:    Translucent
 *    - Two Sided:     true   (visible when the camera is inside the slab)
 *    - Emissive Color = Vector param "ShadowColor"
 *    - Opacity        = Scalar param "ShadowOpacity"  (optionally * a fade scalar)
 *    - (Recommended) sample SceneTexture:CustomStencil; opacity 0 where == 1.
 */
UCLASS()
class THEPERSPVIEW_API AkdCrushShadowVolume : public AActor
{
    GENERATED_BODY()

public:
    AkdCrushShadowVolume();

    // ── Shape (world cm) ──────────────────────────────────────────────────────
    /** Horizontal screen width of the shadow column. */
    UPROPERTY(EditAnywhere, Category = "Crush Shadow|Shape", meta = (ClampMin = "1.0"))
    float ZoneWidthY = 600.f;

    /** Floor-to-ceiling height. */
    UPROPERTY(EditAnywhere, Category = "Crush Shadow|Shape", meta = (ClampMin = "1.0"))
    float ZoneHeightZ = 800.f;

    /** Depth thickness along the camera axis. Only needs to span the play depth. */
    UPROPERTY(EditAnywhere, Category = "Crush Shadow|Shape", meta = (ClampMin = "1.0"))
    float ZoneDepthX = 1200.f;

    // ── Appearance (2D / Crush Mode) ──────────────────────────────────────────
    /** Optional self-rendered tint material. Assign M_CrushShadowVolume to turn
     *  this volume into a per-instance coloured translucent slab. Leave EMPTY to
     *  keep the legacy stencil-only behaviour (global darken via M_CrushPostProcess).
     *  Prefer promoting this to a UkdThemeSettings / DeveloperSettings pointer if
     *  you want one shared default across every volume. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush Shadow|Appearance")
    TObjectPtr<UMaterialInterface> ShadowVolumeMaterial;

    /** Per-volume shadow tint used by the tint slab (Emissive of the Unlit material).
     *  Default approximates the Heliograph IndigoField token (#0E1538). Ignored when
     *  ShadowVolumeMaterial is unset. Authored/re-read on each crush activation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush Shadow|Appearance",
        meta = (HideAlphaChannel))
    FLinearColor ShadowColor = FLinearColor(0.055f, 0.082f, 0.220f, 1.f);

    /** Per-volume shadow strength [0..1] (Opacity of the tint slab).
     *  0 = fully transparent (slab invisible), 1 = solid tint. Ignored when
     *  ShadowVolumeMaterial is unset. Re-read on each crush activation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush Shadow|Appearance",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ShadowOpacity = 0.85f;

    // ── Render ────────────────────────────────────────────────────────────────
    /** Stencil tag the Crush PP material tests. Project convention: 2 = darken. */
    UPROPERTY(EditAnywhere, Category = "Crush Shadow|Render", meta = (ClampMin = "1", ClampMax = "255"))
    int32 ShadowStencilValue = 2;

    /** Keep writing the custom-depth stencil while active. Leave ON so the
     *  neon-preserve pipeline (stencil 1) still resolves correctly even when the
     *  slab supplies the visible darkening. Turn OFF only if you have fully moved
     *  darkening to the slab and no longer want any stencil-2 interaction. */
    UPROPERTY(EditAnywhere, Category = "Crush Shadow|Render")
    bool bWriteShadowStencil = true;

    // ── Depth tracking ────────────────────────────────────────────────────────
    /** Snap depth (X) to the player on Crush entry — aligns with the geometry
     *  UkdGeometryTransitionComponent slides into place. */
    UPROPERTY(EditAnywhere, Category = "Crush Shadow|Depth")
    bool bTrackPlayerDepth = true;

    /** Gap (cm) placed BEHIND the player so the player's stencil-1 wins the depth
     *  test and stays lit inside the shadow. */
    UPROPERTY(EditAnywhere, Category = "Crush Shadow|Depth", meta = (ClampMin = "0.0"))
    float DepthBehindPlayer = 50.f;

    /** Restrict this volume to one collapse axis. Default Any keeps it active on
    *  every crush; set Y-only to make it appear solely on East/West crushes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush Shadow Volume")
    EkdCrushAxisFilter ActivationAxisFilter = EkdCrushAxisFilter::AnyAxis;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

#if WITH_EDITOR
    /** Live-pushes ShadowColor / ShadowOpacity to the dynamic material while a PIE
     *  session is running, so tuning in the Details panel is immediate. */
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    UPROPERTY(VisibleAnywhere, Category = "Crush Shadow")
    TObjectPtr<UStaticMeshComponent> VolumeMesh;

    /** Per-instance material driving ShadowColor / ShadowOpacity. Created at
     *  BeginPlay only when ShadowVolumeMaterial is assigned. */
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> ShadowMID;

    UPROPERTY()
    TObjectPtr<AkdMyPlayer> CachedPlayer;

    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> CachedASC;

    FDelegateHandle CrushTagHandle;
    FVector OriginalPlacedLocation = FVector::ZeroVector;

    void ApplyZoneScale(bool bCollapsesY);
    void SetVolumeActive(bool bActive);

    /** Builds the dynamic material (if a source material is set) and flips the mesh
     *  into the main pass so the translucent slab can render. No-op otherwise. */
    void EnsureDynamicMaterial();

    /** Pushes the current ShadowColor / ShadowOpacity into ShadowMID. Cheap; safe
     *  to call every activation. */
    void RefreshMaterialParameters();

    void OnCrushModeTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

    // ── Material param FName constants ────────────────────────────────────────
    // Change these if you rename the params in the M_CrushShadowVolume material.
    static const FName MP_ShadowColor;
    static const FName MP_ShadowOpacity;
};
