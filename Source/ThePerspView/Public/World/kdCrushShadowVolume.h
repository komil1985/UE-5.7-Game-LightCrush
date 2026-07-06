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
class UkdWorldColorDriver;
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
 *   AAkdGlobalPostProcessVolume's PP_CrushShadowStencil blendable reads that stencil
 *   to darken the scene. In this mode there is NO per-volume colour or opacity.
 *
 * SELF-RENDERED TINT SLAB (opt-in, when ShadowVolumeMaterial is assigned):
 *   The mesh additionally renders itself as an unlit, translucent "tint slab"
 *   whose colour and opacity are driven per-instance by a UMaterialInstanceDynamic.
 *   Set bWriteShadowStencil = false on these volumes so the global stencil-2 darken
 *   ignores them (no double-darken); PP_NeonPreservation still re-brightens stencil-1
 *   neon on top, so lit elements punch through the slab for free.
 *
 * DRIVER-SYNCED DEVELOP-IN FADE (automatic whenever the slab is active):
 *   ShadowOpacity is treated as the *target*. On every crush toggle the slab's live
 *   opacity is driven by UkdWorldColorDriver::GetBlendAlpha() (0 = light, 1 = crush)
 *   so the tint develops in / out on the exact same curve as the rest of the world,
 *   in both directions. This is a READ of the driver's blend state — the driver
 *   remains the sole writer of MPC_WorldColor, so the single-writer rule is intact.
 *
 *   IMPORTANT — do NOT gate this on State.Transitioning. That tag brackets the
 *   geometry morph, which completes *before* State.CrushMode flips and the colour
 *   blend begins; gating on it would freeze the slab at 0 exactly as the fade
 *   should start. The correct clock is the driver's blend (GetBlendAlpha /
 *   IsBlending), which begins on the same tag event this actor already listens to.
 *
 *   The actor ticks only while the driver is blending and idles otherwise
 *   (bStartWithTickEnabled = false; toggled per transition), so at-rest cost is zero.
 *
 *   REQUIRED MATERIAL (M_CrushShadowVolume):
 *    - Shading Model: Unlit ; Blend Mode: Translucent ; Two Sided: true
 *    - Emissive Color = Vector param "ShadowColor"
 *    - Opacity        = Scalar param "ShadowOpacity"
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
     *  keep the legacy stencil-only behaviour (global darken via the PP stack). */
    UPROPERTY(EditDefaultsOnly, Category = "Crush Shadow|Appearance")
    TObjectPtr<UMaterialInterface> ShadowVolumeMaterial;

    /** Per-volume shadow tint (Emissive of the Unlit material). Default approximates
     *  the Heliograph IndigoField token (#0E1538). Ignored when ShadowVolumeMaterial
     *  is unset. Re-read on each crush activation and on edit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush Shadow|Appearance",
        meta = (HideAlphaChannel))
    FLinearColor ShadowColor = FLinearColor(0.055f, 0.082f, 0.220f, 1.f);

    /** Per-volume shadow strength [0..1] — the TARGET opacity at full crush. The live
     *  slab opacity is this value scaled by the world blend alpha, so it develops in
     *  and out with the transition. Ignored when ShadowVolumeMaterial is unset. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush Shadow|Appearance",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ShadowOpacity = 0.85f;

    // ── Render ────────────────────────────────────────────────────────────────
    /** Stencil tag the Crush PP material tests. Project convention: 2 = darken. */
    UPROPERTY(EditAnywhere, Category = "Crush Shadow|Render", meta = (ClampMin = "1", ClampMax = "255"))
    int32 ShadowStencilValue = 2;

    /** Keep writing the custom-depth stencil while active. Set OFF on slab volumes
     *  so the global stencil-2 darken ignores them and the slab is not double-darkened.
     *  Leave ON only when you still want this volume's darkening to come from the
     *  global PP_CrushShadowStencil pass (legacy / stencil-only volumes). */
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
    virtual void Tick(float DeltaTime) override;

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

    /** Read-only handle to the world colour driver on the player pawn. Supplies the
     *  blend alpha the slab fade rides on. Never written to — read only. */
    UPROPERTY()
    TObjectPtr<UkdWorldColorDriver> CachedColorDriver;

    FDelegateHandle CrushTagHandle;
    FVector OriginalPlacedLocation = FVector::ZeroVector;

    // ── Per-crush fade state ──────────────────────────────────────────────────
    /** True if this volume participates in the current crush (axis filter passed). */
    bool bVisibleThisCrush = false;
    /** True while the current target is Crush (entering / held), false while exiting. */
    bool bTargetCrushActive = false;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void ApplyZoneScale(bool bCollapsesY);

    /** Evaluates the axis filter for the current crush direction; if this volume
     *  participates, applies zone scale + depth placement and returns true. */
    bool PositionForCurrentCrush();

    /** Immediate, non-animated state set (level start, or fallback when no tint
     *  material / no driver is available). */
    void SnapToState(bool bInCrush);

    /** Builds the dynamic material (if a source material is set) and flips the mesh
     *  into the main pass so the translucent slab can render. No-op otherwise. */
    void EnsureDynamicMaterial();

    /** Pushes ShadowColor + full ShadowOpacity into the MID (endpoints / editor tune). */
    void RefreshMaterialParameters();

    /** Called when the driver-synced blend has settled: writes the exact endpoint
     *  and disables tick. */
    void FinalizeFade();

    void OnCrushModeTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

    // ── Material param FName constants ────────────────────────────────────────
    // Change these if you rename the params in the M_CrushShadowVolume material.
    static const FName MP_ShadowColor;
    static const FName MP_ShadowOpacity;
};
