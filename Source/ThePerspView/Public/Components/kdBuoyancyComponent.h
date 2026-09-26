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
// water" read, even though these platforms hang in mid-air with nothing
// underneath them.
//
// PHYSICS MODEL
//   A single-axis (world Z) damped harmonic oscillator, stepped every tick
//   with an IMPLICIT (backward-Euler) integrator — unconditionally stable for
//   ANY DeltaTime/Frequency/DampingRatio combination, so it can never explode
//   on a frame hitch. Two independent forces drive it:
//     1. STEADY LOAD    — target snaps to -SinkDepth while a player is on the
//        WeightTrigger, and back to 0 the instant they leave.
//     2. LANDING IMPULSE — the player's downward velocity at touchdown is
//        folded into the spring as an instant velocity kick, so a hard
//        landing plunges it deeper than a gentle step before settling to the
//        same steady equilibrium.
//   A landing impulse can also RIPPLE outward to nearby buoyancy platforms
//   via UkdBuoyancyRippleSubsystem (see RippleRadius / RippleStrength) — an
//   opt-in "connected raft" feel with zero direct references between
//   platforms.
//
// WEIGHT SENSOR — WHY IT'S UNATTACHED
//   The sensor box is deliberately created with NO attach parent, positioned
//   purely in world space from the mesh's bounds at BeginPlay. An earlier
//   version attached it to the owning actor's RootComponent — which silently
//   produced a permanently-stationary platform on ANY actor whose
//   RootComponent is null (this bit AkdFloorBase specifically, which never
//   called SetRootComponent — fixed separately, but this component no longer
//   depends on that being correct either way).
//
// SINGLE-WRITER SAFETY
//   Sole writer of CachedMesh's RelativeLocation.Z offset, UNLESS a sibling
//   UkdGeometryTransitionComponent exists (a floor that ALSO collapses into
//   the shadow plane in Crush Mode). In that case every write SUSPENDS
//   whenever the player's ASC carries State.CrushMode or State.Transitioning
//   — offset is hard-snapped to zero on suspend/resume, never a pop.
//
// RIDING THE PLATFORM
//   CachedMesh must be Movable mobility (logged as a warning if not). Because
//   the mesh IS the walkable collision primitive, UE's own
//   CharacterMovementComponent based-movement automatically carries a
//   standing player down/up with it every tick — the same mechanism as a
//   stock UE elevator, no player-side code required.
//
// SETUP
//   1. Add as a C++ subobject in your platform actor's constructor:
//      CreateDefaultSubobject<UkdBuoyancyComponent>(TEXT("Buoyancy"))
//   2. Leave ManualTriggerExtent at (0,0,0) to auto-fit the weight sensor to
//      the mesh's world bounds on BeginPlay.
//   3. Tune SinkDepth / Frequency / DampingRatio per platform instance.
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
     *  instant downward velocity kick on touchdown. 0 disables the impulse
     *  layer entirely (steady sink only). */
    UPROPERTY(EditAnywhere, Category = "Buoyancy | Impulse", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ImpulseToVelocityScale = 0.35f;

    /** Landing speeds above this (cm/s) are clamped before scaling, so a very
     *  hard fall can't punch the platform through its own floor. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy | Impulse", meta = (ClampMin = "0.0"))
    float MaxLandingSpeedForImpulse = 1400.f;

    // ── Ripple (connected-raft feel) ─────────────────────────────────────────

    /** How far (cm) a landing impulse on THIS platform ripples out to
     *  neighboring buoyancy platforms. 0 (default) disables rippling — a
     *  platform still registers so OTHERS can reach it, it just never emits. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy | Ripple", meta = (ClampMin = "0.0"))
    float RippleRadius = 0.f;

    /** Fraction of this platform's own impulse passed to a neighbor at
     *  distance 0, linearly fading to 0 at RippleRadius. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy | Ripple", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RippleStrength = 0.4f;

    // ── Weight sensor ─────────────────────────────────────────────────────────

    /** Non-zero = use this box extent verbatim instead of auto-fitting to the
     *  mesh's world bounds. Use for a rotated or irregularly-shaped platform. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy | Weight Sensor")
    FVector ManualTriggerExtent = FVector::ZeroVector;

    /** World-space offset applied on top of the computed sensor position —
     *  lets you nudge it without fully overriding the size. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy | Weight Sensor")
    FVector TriggerLocationOffset = FVector::ZeroVector;

    /** Extra vertical margin (cm) the auto-fit box extends above the mesh's
     *  top face, so a capsule resting exactly on the surface still overlaps. */
    UPROPERTY(EditAnywhere, Category = "Buoyancy | Weight Sensor", meta = (ClampMin = "0.0"))
    float AutoFitTopMargin = 40.f;

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

    /** World location of the mesh this component drives — used by
     *  UkdBuoyancyRippleSubsystem to compute inter-platform distance. */
    FVector GetMeshWorldLocation() const;

    /** Injects an external velocity kick into this platform's own spring.
     *  Never touches the steady load target — a ripple is a transient
     *  shudder, not a fake occupancy. */
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

    /** Sizes/positions WeightTrigger from CachedMesh's WORLD bounds — no
     *  dependency on the owner's root component or hierarchy. */
    void AutoFitWeightTrigger();

    /** True while a sibling UkdGeometryTransitionComponent owns the mesh's
     *  transform (Crush Mode active or transitioning) on this actor. */
    bool IsSuspendedForCrushHandoff() const;

    /** Steps the implicit spring-damper one tick. Unconditionally stable. */
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
    bool  bWasSuspended = false;
};