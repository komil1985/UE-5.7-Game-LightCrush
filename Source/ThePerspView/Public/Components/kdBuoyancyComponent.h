// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdBuoyancyComponent.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UkdGeometryTransitionComponent;
class UkdBuoyancyRippleSubsystem;
class USoundBase;

// ─────────────────────────────────────────────────────────────────────────────
// UkdBuoyancyComponent
//
// Drop onto any floating platform actor whose root is a single
// UStaticMeshComponent acting as BOTH the walkable collision AND the visual
// mesh. While the player stands on it, the mesh sinks toward a lower
// equilibrium and springs back the instant they leave — the "log floating on
// water" read. Works identically in 3D and in 2D Crush Mode.
//
// PHYSICS MODEL
//   A single-axis (world Z) damped harmonic oscillator, stepped every tick
//   with an IMPLICIT (backward-Euler) integrator — unconditionally stable for
//   ANY DeltaTime/Frequency/DampingRatio. Two forces drive it:
//     1. STEADY LOAD     — target = -SinkDepth while a player is on the sensor.
//     2. LANDING IMPULSE — the player's downward speed at touchdown is folded
//        into the spring as an instant velocity kick.
//   Impulses can RIPPLE to nearby platforms via UkdBuoyancyRippleSubsystem.
//
// SINGLE-WRITER OWNERSHIP
//   • No sibling UkdGeometryTransitionComponent → this component writes the
//     mesh's RelativeLocation.Z directly.
//   • Sibling present (floor that collapses in Crush Mode) → this component
//     NEVER writes the mesh. It only hands its spring offset to the geometry
//     component (SetExternalZOffset), which stays the sole writer and layers
//     the offset onto every shiver / morph / rest write. The sink therefore
//     carries seamlessly through a crush transition — no suspend, no pop, no
//     two-writer fight.
//
// WEIGHT SENSOR
//   Unattached, world-space, fitted from the mesh's CURRENT world bounds with
//   the live sink offset removed (= the platform's rest pose). It is refitted
//   whenever the geometry component finishes a morph, so in Crush Mode it
//   follows the collapsed slab; the collapse axis (resolved via
//   MakeCrushBasis) is thickened to CrushPlaneMinHalfThickness so a capsule
//   centred on the plane always overlaps it.
//
// RIDING THE PLATFORM
//   The mesh must be Movable. UE's CharacterMovementComponent based-movement
//   carries a WALKING player with the mesh (same as a stock UE elevator).
//   A gravity-off shadow-mode player has no movement base, so he is not carried.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEPERSPVIEW_API UkdBuoyancyComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UkdBuoyancyComponent();

    // ── Spring tuning ─────────────────────────────────────────────────────────

    /** How far (cm) the platform sinks at rest while a player stands on it. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy", meta = (ClampMin = "0.0"))
    float SinkDepth = 18.f;

    /** Oscillation speed (Hz). Higher = snappier response. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy", meta = (ClampMin = "0.05", ClampMax = "10.0"))
    float Frequency = 1.4f;

    /** 0 = bounces forever, 1 = settles with no overshoot, >1 = sluggish.
     *  0.3-0.6 gives one satisfying dip-and-rebound, like a real buoy. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy", meta = (ClampMin = "0.0", ClampMax = "3.0"))
    float DampingRatio = 0.45f;

    // ── Landing impulse ───────────────────────────────────────────────────────

    /** Fraction of the player's landing speed (cm/s) fed into the spring as an
     *  instant downward velocity kick on touchdown. 0 = steady sink only. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy | Impulse", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ImpulseToVelocityScale = 0.35f;

    /** Landing speeds above this (cm/s) are clamped before scaling. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy | Impulse", meta = (ClampMin = "0.0"))
    float MaxLandingSpeedForImpulse = 1400.f;

    // ── Ripple (connected-raft feel) ─────────────────────────────────────────

    /** How far (cm) a landing impulse on THIS platform ripples to neighbors.
     *  0 (default) = never emits, but still receives from others. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy | Ripple", meta = (ClampMin = "0.0"))
    float RippleRadius = 0.f;

    /** Fraction of this platform's impulse passed to a neighbor at distance 0,
     *  linearly fading to 0 at RippleRadius. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy | Ripple", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RippleStrength = 0.4f;

    // ── Weight sensor ─────────────────────────────────────────────────────────

    /** Non-zero X/Y = use these horizontal half-extents instead of the mesh's
     *  bounds. 3D pose only — ignored while collapsed in Crush Mode. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy | Weight Sensor")
    FVector ManualTriggerExtent = FVector::ZeroVector;

    /** World-space nudge applied to the sensor. 3D pose only. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy | Weight Sensor")
    FVector TriggerLocationOffset = FVector::ZeroVector;

    /** Extra vertical margin (cm) above the mesh's top face. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy | Weight Sensor", meta = (ClampMin = "0.0"))
    float AutoFitTopMargin = 40.f;

    /** Minimum half-thickness (cm) of the sensor along the collapse axis while
     *  the floor is flattened in Crush Mode. Must exceed the capsule radius. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy | Weight Sensor", meta = (ClampMin = "1.0"))
    float CrushPlaneMinHalfThickness = 60.f;

    // ── Feedback hooks ────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, Category = "Buoyancy | Feedback")
    TObjectPtr<USoundBase> LoadSound;

    UPROPERTY(EditDefaultsOnly, Category = "Buoyancy | Feedback")
    TObjectPtr<USoundBase> UnloadSound;

    UFUNCTION(BlueprintImplementableEvent, Category = "Buoyancy")
    void BP_OnPlatformLoaded();

    UFUNCTION(BlueprintImplementableEvent, Category = "Buoyancy")
    void BP_OnPlatformUnloaded();

    /** Fired when a NEARBY platform's landing impulse ripples into this one. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Buoyancy")
    void BP_OnRippleReceived(float VelocityKick);

    // ── Ripple subsystem API ──────────────────────────────────────────────────

    FVector GetMeshWorldLocation() const;

    /** Injects an external velocity kick into this platform's spring. Never
     *  touches the steady load target — a ripple is a transient shudder. */
    void ReceiveRippleImpulse(float VelocityKick);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UFUNCTION()
    void OnWeightTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnWeightTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    /** Waits out any sibling morph, then refits the sensor once it settles. */
    void UpdateWeightTriggerFit();

    /** Rebuilds the sensor from the mesh's current world bounds (rest pose),
     *  for whichever pose — 3D or collapsed 2D — the floor is in right now. */
    void FitWeightTriggerToRestPose();

    /** Pushes DisplacementZ to the mesh — directly, or via the sibling geometry
     *  component when one exists (single-writer). */
    void ApplyDisplacement();

    static void StepSpring(float& X, float& V, float InTarget, float InDampingRatio, float InFrequencyHz, float InDeltaTime);

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> CachedMesh;

    UPROPERTY()
    TObjectPtr<UBoxComponent> WeightTrigger;

    UPROPERTY()
    TObjectPtr<UkdGeometryTransitionComponent> SiblingGeometryTransition;

    TWeakObjectPtr<AActor> OverlappingPlayer;

    FVector OriginalMeshRelativeLoc = FVector::ZeroVector;

    float DisplacementZ = 0.f;   // current spring offset — <= 0 while loaded
    float VelocityZ = 0.f;
    bool  bIsLoaded = false;
    bool  bTriggerDirty = true;  // sensor needs a refit once the mesh is settled
};