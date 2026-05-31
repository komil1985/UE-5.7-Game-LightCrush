// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "kdShadowZone.generated.h"

class UBoxComponent;
class UPostProcessComponent;
class UMaterialInstanceDynamic;
class UCurveFloat;

/**
 * AkdShadowZone — Crush Mode volumetric shadow atmosphere.
 *
 * REPLACES AkdShadow2DPlane.  Zero visible geometry — pure post-process.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * HOW IT WORKS
 * ─────────────────────────────────────────────────────────────────────────────
 * A UPostProcessComponent (bUnbound = true, Priority = 2) drives a custom PP
 * material (M_ShadowZone) via an animated BlendWeight.  At BlendWeight = 0 the
 * renderer skips the material entirely — zero GPU cost in 3D mode.
 *
 * The material stacks three independent shadow layers:
 *
 *   1. HORIZONTAL MASK
 *      smoothstep fade at the zone's world-Y boundaries, visible as a soft
 *      penumbra between lit corridors and the shadow column in 2D mode.
 *      C++ projects zone Y extents to screen UV each tick via
 *      UpdateScreenProjection() and writes ScreenLeft / ScreenRight /
 *      FadeWidth to the DynMI.  As the player moves, the zone tracks.
 *
 *   2. VERTICAL GRADIENT
 *      Ceiling dark (TopDarkness ≈ 0.08), floor lighter (BottomDarkness ≈ 0.28).
 *      A power curve (VertGradExponent) controls how sharply the darkness
 *      concentrates near the top — higher values push the deep shadow
 *      overhead, making the floor feel like dim ambient light pools there.
 *
 *   3. SCREEN VIGNETTE
 *      Radial dark halo at screen corners.  Does NOT require geometry or
 *      spatial masking — it communicates "you are deep inside shadow" as a
 *      pure cinematic layer on top of everything else.
 *
 *   + COOL TINT
 *      Blue-grey shift applied inside the horizontal mask only, so the lit
 *      world outside the zone stays warm.
 *
 * BlendWeight animates 0 → 1 on State_CrushMode tag entry, 1 → 0 on exit.
 * Supports the same UCurveFloat easing pattern as every other system.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * MATERIAL  M_ShadowZone
 * ─────────────────────────────────────────────────────────────────────────────
 *  Settings in UE5 Material Editor:
 *    Material Domain     : Post Process
 *    Blendable Location  : Before Tonemapping
 *
 *  Node connections:
 *    SceneTexture(PostProcessInput0) → RGB  → Custom node "SceneColor" input
 *    ScreenPosition (Viewport UV)           → Custom node "ScreenUV"  input
 *    All scalar parameters (see MP_* list)  → matching Custom node input pins
 *    "ShadowTint" VectorParameter → RGB     → Custom node "ShadowTint" input
 *    Custom node output                     → EmissiveColor
 *
 *  Custom HLSL (copy verbatim — all param names must match MP_* constants):
 *  ─────────────────────────────────────────────────────────────────────────
 *  float sX = ScreenUV.x;
 *  float sY = ScreenUV.y;   // 0 = top (ceiling), 1 = bottom (floor)
 *
 *  // 1. Horizontal mask with smooth penumbra
 *  float leftEdge  = smoothstep(ScreenLeft  - FadeWidth, ScreenLeft  + FadeWidth, sX);
 *  float rightEdge = 1.0 - smoothstep(ScreenRight - FadeWidth, ScreenRight + FadeWidth, sX);
 *  float hMask = saturate(leftEdge * rightEdge);
 *
 *  // 2. Vertical gradient — ceiling darkest, floor lighter
 *  float vt       = pow(saturate(1.0 - sY), VertGradExp);
 *  float darkMult = lerp(BottomDarkness, TopDarkness, vt);
 *
 *  // 3. Screen-edge vignette
 *  float2 vc  = ScreenUV - 0.5;
 *  float vigR = length(vc * float2(1.6, 1.25));
 *  float vig  = smoothstep(0.18, 0.75, vigR);
 *  float vigMult = lerp(1.0, VignetteDark, vig);
 *
 *  // 4. Tint + darkness
 *  float3 tinted   = SceneColor * lerp(1.0, ShadowTint, TintStrength);
 *  float3 shadowed = tinted * darkMult * vigMult;
 *
 *  // 5. Blend inside mask only
 *  float3 final = lerp(SceneColor, shadowed, hMask);
 *  return float4(final, 1.0);
 *  ─────────────────────────────────────────────────────────────────────────
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * SETUP (per actor)
 * ─────────────────────────────────────────────────────────────────────────────
 *  1. Place actor at world X = CrushWorldX (usually 0), vertically centred
 *     between floor and ceiling (midpoint of room height).
 *  2. Set YHalfExtent to the lateral width of the shadow corridor.
 *  3. Set ZHalfExtent to half the room height (floor-to-ceiling / 2).
 *  4. Assign M_ShadowZone to ShadowPostProcessMaterial in the Details panel.
 *  5. Tune TopDarkness / BottomDarkness / EdgeFadeWorldUnits to taste.
 *  6. Actor auto-registers for State_CrushMode on the local player's ASC.
 *
 * NOTE ON PRIORITY:
 *  UkdWorldColorDriver runs its PPComponent at Priority = 1000.
 *  This actor runs at Priority = 2.  The WorldColorDriver handles global
 *  tone (white balance, bloom, vignette).  This actor handles the spatial
 *  shadow column.  They operate independently via WeightedBlendables.
 */
UCLASS()
class THEPERSPVIEW_API AkdShadowZone : public AActor
{
    GENERATED_BODY()

public:
    AkdShadowZone();

    // ── Zone Dimensions ───────────────────────────────────────────────────────

    /** X half-extent. Editor visualisation only — no gameplay significance.
     *  Keep at default (200 cm) unless you want a wider editor wireframe. */
    UPROPERTY(EditAnywhere, Category = "Shadow Zone | Bounds",
        meta = (ClampMin = "10.0"))
    float XHalfExtent = 200.f;

    /** Half-width of the shadow corridor in world Y. */
    UPROPERTY(EditAnywhere, Category = "Shadow Zone | Bounds",
        meta = (ClampMin = "100.0"))
    float YHalfExtent = 1500.f;

    /** Half-height of the zone (floor-to-ceiling height / 2).
     *  Place the actor at mid-room Z so the zone covers both ceiling and floor. */
    UPROPERTY(EditAnywhere, Category = "Shadow Zone | Bounds",
        meta = (ClampMin = "100.0"))
    float ZHalfExtent = 2500.f;

    /** World-unit width of the soft penumbra at zone Y-axis boundaries.
     *  Projected to screen UV each tick.  100–300 cm produces a clean edge
     *  — wider feels like fog diffusion, narrower feels like a hard cut. */
    UPROPERTY(EditAnywhere, Category = "Shadow Zone | Bounds",
        meta = (ClampMin = "0.0"))
    float EdgeFadeWorldUnits = 120.f;

    UPROPERTY(EditDefaultsOnly, Category = "Shadow Zone | Bounds",
        meta = (ClampMin = "5.0", ClampMax = "40.0"))
    float Crush2DFOV = 14.f;

    // ── Material ──────────────────────────────────────────────────────────────

    /** Assign M_ShadowZone (Post Process, Before Tonemapping) here.
     *  A UMaterialInstanceDynamic is created from this at BeginPlay so
     *  parameters can be driven at runtime. */
    UPROPERTY(EditDefaultsOnly, Category = "Shadow Zone | Material")
    TObjectPtr<UMaterialInterface> ShadowPostProcessMaterial;

    // ── Darkness ──────────────────────────────────────────────────────────────

    /** Scene colour multiplier at screen top (ceiling).
     *  0.0 = pure black overhead, 0.08 = deep oppressive shadow.
     *  Must be ≤ BottomDarkness. */
    UPROPERTY(EditDefaultsOnly, Category = "Shadow Zone | Darkness",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TopDarkness = 0.08f;

    /** Scene colour multiplier at screen bottom (floor).
     *  0.25–0.35 reads as "moody shadow you can still navigate in".
     *  Values below 0.15 become difficult to read at floor level. */
    UPROPERTY(EditDefaultsOnly, Category = "Shadow Zone | Darkness",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BottomDarkness = 0.28f;

    /** Power applied to the vertical gradient curve.
     *  1.0 = linear.  1.5–2.5 = darkness concentrates near ceiling.
     *  Higher values exaggerate the "shadows pool overhead" feel. */
    UPROPERTY(EditDefaultsOnly, Category = "Shadow Zone | Darkness",
        meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float VertGradExponent = 1.8f;

    /** Brightness at screen corners inside the zone.
     *  0.0 = black vignette corners, 0.18 = subtle cinematic darkening.
     *  Applied on top of the vertical gradient — does not interact with hMask. */
    UPROPERTY(EditDefaultsOnly, Category = "Shadow Zone | Darkness",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float VignetteDarkness = 0.18f;

    /** Cool blue-grey tint applied inside the shadow zone.
     *  (0.65, 0.72, 1.0) → moonlit shadow cast.  Use (1,1,1) to disable. */
    UPROPERTY(EditDefaultsOnly, Category = "Shadow Zone | Darkness")
    FLinearColor ShadowTint = FLinearColor(0.65f, 0.72f, 1.0f, 1.f);

    /** 0 = no tint shift, 1 = fully tinted.
     *  0.18–0.22 is subtle enough to feel atmospheric without looking filtered. */
    UPROPERTY(EditDefaultsOnly, Category = "Shadow Zone | Darkness",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TintStrength = 0.20f;

    // ── Fade ─────────────────────────────────────────────────────────────────

    /** Seconds for the shadow to fade in/out on CrushMode toggle. */
    UPROPERTY(EditDefaultsOnly, Category = "Shadow Zone | Fade",
        meta = (ClampMin = "0.05"))
    float FadeDuration = 0.35f;

    /** Optional easing curve for the BlendWeight fade.
     *  Null = linear.  Same UCurveFloat workflow as the rest of the system. */
    UPROPERTY(EditDefaultsOnly, Category = "Shadow Zone | Fade")
    TObjectPtr<UCurveFloat> FadeAlphaCurve;

    // ── Stencil (neon colour-preserve pipeline) ───────────────────────────────

    /** X-axis half-thickness of the stencil detection slab (cm).
     *  Kept thin so only actors at the crush plane X receive the stencil,
     *  preventing false positives from geometry further away on X. */
    UPROPERTY(EditAnywhere, Category = "Shadow Zone | Stencil",
        meta = (ClampMin = "1.0"))
    float VolumeThickness = 80.f;

    /** Custom depth stencil value written to actors inside the detection slab.
     *  Convention: 1 = neon preserve (UkdNeonVisibilityComponent),
     *              2 = shadow zone (this actor). */
    UPROPERTY(EditDefaultsOnly, Category = "Shadow Zone | Stencil",
        meta = (ClampMin = "1", ClampMax = "255"))
    int32 ShadowStencilValue = 2;

    // ── Blueprint hooks ───────────────────────────────────────────────────────

    UFUNCTION(BlueprintImplementableEvent, Category = "Shadow Zone")
    void BP_OnShadowFadeInComplete();

    UFUNCTION(BlueprintImplementableEvent, Category = "Shadow Zone")
    void BP_OnShadowFadeOutComplete();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    // ── Components ────────────────────────────────────────────────────────────

    /** Blue wireframe in editor only.  No collision, no rendering.
     *  Also serves as the anchor for StencilDetectionBox's relative placement. */
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> ZoneBoundsBox;

    /** bUnbound = true: always applies globally at BlendWeight = 0 (free).
     *  BlendWeight is animated 0 → 1 during crush mode entry and 1 → 0 on exit. */
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UPostProcessComponent> ShadowPostProcess;

    /** Thin slab (VolumeThickness × YHalfExtent × ZHalfExtent) at actor origin X.
     *  Overlap-only.  Stamps ShadowStencilValue onto actors at the crush plane
     *  for the neon colour-preserve post-process pipeline. */
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> StencilDetectionBox;

    // ── Runtime ───────────────────────────────────────────────────────────────

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> DynMI;

    float FadeAlpha = 0.f;  // normalised [0,1]
    float FadeDirection = 0.f;  // +1 fading in, -1 fading out, 0 idle
    bool  bCrushActive = false;
    bool  bOverlapBound = false;

    // ── Internal ──────────────────────────────────────────────────────────────

    void InitialiseComponents();
    void InitialiseMaterialParameters();
    void RegisterCrushModeTag();

    void OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount);

    // ── Stencil ───────────────────────────────────────────────────────────────

    void SetVolumeOverlapEventsActive(bool bActive);
    void SetStencilOnOverlappingActors(bool bEnable);
    void SetActorShadowStencil(AActor* TargetActor, bool bEnable) const;

    UFUNCTION()
    void OnActorEnterStencilVolume(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnActorExitStencilVolume(UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    // ── Material param FName constants ────────────────────────────────────────
    // Change these if you rename the params in the M_ShadowZone material.
    static const FName MP_TanHalfHFOV;
    static const FName MP_ZoneCentreY;
    static const FName MP_ZoneHalfWidth;
    static const FName MP_ZoneFadeWidth;
    static const FName MP_ZonePlaneX;
    static const FName MP_TopDarkness;
    static const FName MP_BottomDarkness;
    static const FName MP_VertGradExp;
    static const FName MP_VignetteDark;
    static const FName MP_TintStrength;
    static const FName MP_ShadowTint;
};
