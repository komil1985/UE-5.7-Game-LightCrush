// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdGeometryTransitionComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// UkdGeometryTransitionComponent
//
// Drop on any floor, wall or platform actor. Subscribes to the player's
// State.CrushMode tag.  When the mode changes it runs a three-step sequence:
//
//   1. SHIVER  — rapid Y/Z oscillation, amplitude decays to zero.
//                Reads as the geometry "cracking" before it moves.
//
//   2. MORPH   — lerps the actor's X world position toward the crush plane and
//                squashes its X scale to CrushXScaleMultiplier.
//                Result: geometry slides flat onto the shadow plane and
//                becomes paper-thin — a visible 3D → 2D world shift.
//                On crush EXIT the sequence runs in reverse.
//
//   3. SNAP    — at morph end, values are hard-set and tick is disabled.
//
// FLOORS vs WALLS — same component, one knob:
//   • Floors / platforms : CrushDepthOffset = 0  → collapse onto the player's
//                          plane (the original behaviour).
//   • Walls / backdrops  : CrushDepthOffset > 0  → collapse a few steps BEHIND
//                          the player so the (off-plane) collision can never
//                          block the plane-constrained capsule.
//
// SHADOW — bFlattenShadowInCrush suppresses the mesh's 3D shadow casting while
//   flattened so the shadow reads flat on the 2D stage exactly like the floors;
//   the designer's original shadow settings are restored on exit.
//
// SETUP
//   1. Add component to BP_Floor / BP_Wall / BP_Platform.
//   2. Set CrushWorldX to the X coordinate of your shadow plane (usually 0).
//   3. For walls, set CrushDepthOffset larger than the player capsule radius.
//   4. Tune ShiverAmplitude and CrushXScaleMultiplier in Details.
//   5. No curve assets required — fully self-contained tick math.
// ─────────────────────────────────────────────────────────────────────────────

class UStaticMeshComponent;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEPERSPVIEW_API UkdGeometryTransitionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UkdGeometryTransitionComponent();

    // ── Morph targets ─────────────────────────────────────────────────────────

/** World X coordinate the geometry slides to in Crush (2D) mode.
 *  Set to the X position of your shadow wall — usually 0. */
    UPROPERTY(EditInstanceOnly, Category = "Crush | Geometry")
    float CrushWorldX = 0.f;

    /** How thin the geometry's X axis becomes in Crush mode.
     *  0.04 = 4 % of original X extent (paper-thin slab). */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Geometry",
        meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float CrushXScaleMultiplier = 0.04f;

    /** Distance (cm) to push this geometry BEHIND the player's stage plane in Crush.
    *  0   -> flush with the plane (floors, platforms, shadow areas).
    *  >0  -> sits this far behind the player so it never blocks movement (walls).
    *  Use a value larger than the player capsule radius (+ a margin) for walls.
    *  When bAutoOffsetBehindCamera is true only the MAGNITUDE matters — the sign
    *  is resolved automatically to the side away from the Crush camera. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Geometry",
        meta = (UIMin = "0.0", UIMax = "400.0"))
    float CrushDepthOffset = 0.f;

    /** If true, CrushDepthOffset is applied away from the Crush camera, so a
     *  positive value is always "behind" the player regardless of which side the
     *  camera sits on. If false, CrushDepthOffset is a raw signed +X delta. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Geometry")
    bool bAutoOffsetBehindCamera = true;

    /** Seconds for the main slide + squash. Slightly longer than the player
     *  transition gives the world a satisfying "catching up" lag. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Geometry",
        meta = (ClampMin = "0.1"))
    float MorphDuration = 0.6f;

    // ── Shadow ──────────────────────────────────────────────────────────────────

    /** In Crush Mode, stop this geometry casting 3D shadows so the shadow reads
    *  flat on the 2D stage (matches the AkdCrushShadowVolume cast-shadow rule).
    *  The original CastShadow / bCastHiddenShadow values are restored on exit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Geometry | Shadow")
    bool bFlattenShadowInCrush = true;

    // ── Shiver ────────────────────────────────────────────────────────────────

    /** Duration of the shiver phase before the main slide (seconds).
     *  Set to 0 to skip. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Geometry | Shiver",
        meta = (ClampMin = "0.0", ClampMax = "0.3"))
    float ShiverDuration = 0.09f;

    /** Peak Y/Z displacement of the shiver (cm). */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Geometry | Shiver",
        meta = (ClampMin = "0.0", ClampMax = "40.0"))
    float ShiverAmplitude = 7.f;

    /** Oscillation cycles per second during shiver. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Geometry | Shiver",
        meta = (ClampMin = "1.0", ClampMax = "60.0"))
    float ShiverFrequency = 24.f;

protected:
	virtual void BeginPlay() override;	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UFUNCTION()
    void OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount);

    /** Resolve the signed X offset to apply behind ReferenceX (the plane/player X). */
    float ResolveDepthOffset(const FVector& CollapseNormal, float ReferenceOnAxis) const;

    /** Toggle 3D shadow casting on the cached mesh (no-op if bFlattenShadowInCrush is false). */
    void ApplyShadowFlatten(bool bFlatten);

    /** Recompute the crush-target transform from the player's active crush
    *  direction. Called on every Crush entry (and on boot-into-crush). */
    void RecomputeCrushTarget();

    // ── Cached mesh (first UStaticMeshComponent on the owner) ─────────────────
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> CachedMesh;

    // ── Internal state machine ────────────────────────────────────────────────

    enum class EGeoState : uint8 { Idle, Shivering, Morphing };
    EGeoState State = EGeoState::Idle;
    bool      bToCrushMode = false;
    float     StateElapsed = 0.f;

    // ── Baselines captured at BeginPlay ───────────────────────────────────────

    FVector OriginalMeshRelativeLoc;
    FVector OriginalMeshRelativeScale;

    FVector MeshRelLocCrush = FVector::ZeroVector;  // mesh-relative location at full crush
    FVector MeshRelScaleCrush = FVector::OneVector;   // mesh-relative scale at full crush

    // Designer shadow settings, restored when leaving Crush.
    bool bOrigCastShadow = true;
    bool bOrigCastHiddenShadow = false;
};
