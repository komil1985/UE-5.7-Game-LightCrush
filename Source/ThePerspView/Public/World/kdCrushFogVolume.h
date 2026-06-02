// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "kdCrushFogVolume.generated.h"

class ULocalFogVolumeComponent;
class UBoxComponent;
class UCurveFloat;
class AkdMyPlayer;

// ─────────────────────────────────────────────────────────────────────────────
// AkdCrushFogVolume
//
// Drop in any level that uses Crush Mode.  When State.CrushMode becomes active
// this actor fades a volumetric fog slab into view, darkening the full vertical
// slice at the shadow-plane position.
//
// HOW THE SHAPE WORKS
//   ULocalFogVolumeComponent is an analytic sphere of radius 500 cm (immutable,
//   from GetBaseVolumeSize()).  Non-uniform actor scale stretches it into the
//   slab ellipsoid we need:
//     thin on X  → slab depth       (80 cm default)
//     wide on Y  → level width      (3000 cm default)
//     tall on Z  → floor-to-ceiling (2000 cm default)
//   The editor UBoxComponent has BoxExtent=(500,500,500) and inherits the same
//   actor scale, so its outline matches the fog slab exactly in the viewport.
//
// WHY BETTER THAN AkdShadow2DPlane
//   • Volumetric fog integrates into UE's lighting pipeline — not a screen-
//     space multiply.  Emissive and neon materials naturally glow through it
//     (emissive is added after volumetric scattering, not multiplied by it).
//   • No Z-fighting, no translucent sort-order artifacts, no visible plane edge.
//   • Passively solves neon colour preservation with zero stencil work.
//
// PREREQUISITES (one-time per level)
//   1.  Place an AExponentialHeightFog actor with "Volumetric Fog" enabled.
//   2.  DefaultEngine.ini → [/Script/Engine.RendererSettings]:
//         r.VolumetricFog=1
//
// RUNTIME API — ALL CHANGES GO THROUGH SETTER FUNCTIONS
//   ULocalFogVolumeComponent in UE5.7 requires setter calls (SetRadialFogExtinction,
//   SetFogAlbedo, etc.) for changes to propagate to the render proxy immediately.
//   Direct property writes on registered components skip the render thread dispatch.
// ─────────────────────────────────────────────────────────────────────────────

UCLASS(Blueprintable, meta = (DisplayName = "Crush Fog Volume"))
class THEPERSPVIEW_API AkdCrushFogVolume : public AActor
{
    GENERATED_BODY()

public:
    AkdCrushFogVolume();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ── Components ────────────────────────────────────────────────────────────

    /** Root component.  Actor scale shapes this sphere into the fog slab.
     *  Base sphere radius is immutable at 500 cm (GetBaseVolumeSize()). */
    UPROPERTY(EditDefaultsOnly, Category = "Components")
    TObjectPtr<ULocalFogVolumeComponent> FogVolume;

    /** Editor-only box visualizing slab bounds.  BoxExtent=(500,500,500)
     *  inherits actor scale → matches the fog ellipsoid exactly. */
    UPROPERTY(EditDefaultsOnly, Category = "Components")
    TObjectPtr<UBoxComponent> BoundsViz;

    // ── Shape ─────────────────────────────────────────────────────────────────

    /** Half-depth of the fog slab along X (into the 3D world, cm).
     *  80 cm reads as a tight shadow wall.  Increase for a wider blur zone. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Fog | Shape",
        meta = (ClampMin = "1.0"))
    float VolumeHalfExtentX = 80.f;

    /** Half-width along Y (level playfield width).
     *  Size this to cover the widest part of the level. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Fog | Shape",
        meta = (ClampMin = "1.0"))
    float VolumeHalfExtentY = 3000.f;

    /** Half-height along Z (floor-to-ceiling + headroom). */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Fog | Shape",
        meta = (ClampMin = "1.0"))
    float VolumeHalfExtentZ = 2000.f;

    // ── Fog appearance ────────────────────────────────────────────────────────

    /** Peak radial extinction coefficient when Crush Mode is fully active.
     *  Maps to ULocalFogVolumeComponent::RadialFogExtinction.
     *  0.08 = soft atmospheric haze, 0.12 = default shadow, 0.20+ = near-opaque.
     *  This is the ONLY property that changes per-tick during the fade. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Fog | Appearance",
        meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float PeakRadialExtinction = 0.12f;

    /** Scattering colour.  Near-black with a faint indigo tint for the
     *  Limbo / Liminal Dusk aesthetic.  Clamped to [0,1] by the component. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Fog | Appearance")
    FLinearColor FogAlbedoColor = FLinearColor(0.025f, 0.012f, 0.050f, 1.f);

    /** Faint self-illumination — gives the slab a ghost-like inner glow.
     *  Set to FLinearColor::Black to disable.  Clamped to [0,1] by the component. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Fog | Appearance")
    FLinearColor FogEmissiveColor = FLinearColor(0.004f, 0.002f, 0.012f, 1.f);

    /** Phase G parameter for directional scattering.
     *  0 = isotropic (uniform shadow), >0 = forward-scattering.
     *  Keep at 0 for a clean shadow slab with no directional bias. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Fog | Appearance",
        meta = (ClampMin = "0.0", ClampMax = "0.999"))
    float ScatteringPhaseG = 0.f;

    // ── Transition ────────────────────────────────────────────────────────────

    /** Seconds to fade fully in or out when the CrushMode tag changes. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Fog | Transition",
        meta = (ClampMin = "0.05"))
    float FadeDuration = 0.4f;

    /** Optional curve remapping normalised time [0,1] to a custom easing shape.
     *  Null = linear.  The curve is evaluated in Tick before calling
     *  ApplyFogExtinction — do NOT call GetFloatValue again inside that function
     *  or the curve will be double-applied. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Fog | Transition")
    TObjectPtr<UCurveFloat> FadeCurve;

    // ── Blueprint events ──────────────────────────────────────────────────────

    /** Fires once the fog has faded fully in (alpha == 1). */
    UFUNCTION(BlueprintImplementableEvent, Category = "Crush | Fog")
    void BP_OnFogFadeInComplete();

    /** Fires once the fog has faded fully out (alpha == 0). */
    UFUNCTION(BlueprintImplementableEvent, Category = "Crush | Fog")
    void BP_OnFogFadeOutComplete();

private:
    // ── Runtime state ─────────────────────────────────────────────────────────

    float CurrentAlpha = 0.f;   // normalised [0,1] fade progress
    float FadeDirection = 0.f;   // +1 fading in  |  -1 fading out  |  0 idle
    bool  bCrushActive = false;

    FDelegateHandle CrushTagHandle;

    /** Cached player ref — used to read worldX on each Crush entry so the slab
     *  repositions to the shadow plane before fading in. */
    UPROPERTY()
    TObjectPtr<AkdMyPlayer> CachedPlayer;

    // ── Helpers ───────────────────────────────────────────────────────────────

    /** Applies VolumeHalfExtentX/Y/Z to actor scale so the fog sphere fits
     *  the intended slab shape.  Called once in BeginPlay. */
    void SizeVolumeFromExtents();

    /** Calls SetRadialFogExtinction(EvaluatedAlpha * PeakRadialExtinction).
     *  Using the setter is required for runtime changes to reach the render proxy. */
    void ApplyFogExtinction(float EvaluatedAlpha);

    /** GAS tag callback — registered in BeginPlay, removed in EndPlay. */
    UFUNCTION()
    void OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount);
};
