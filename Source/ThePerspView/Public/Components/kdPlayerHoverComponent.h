// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdPlayerHoverComponent.generated.h"

    // ─── Blink State ──────────────────────────────────────────────────────────

UENUM()
enum class EBlinkPhase : uint8
{
    Idle,       // waiting for the next scheduled blink
    Closing,    // eye scale shrinking toward BlinkClosedScale
    Holding,    // eye held closed for BlinkHoldTime
    Opening,    // eye scale returning to full
};

// ─────────────────────────────────────────────────────────────────────────────
// UkdPlayerHoverComponent
//
// Cosmetic-only component attached to AkdMyPlayer that drives two effects:
//
//   1. HOVER OSCILLATION
//      A sine-wave vertical bob applied to the skeletal mesh relative offset
//      (capsule stays put — physics / collision unaffected).
//      • Full amplitude while idle.
//      • Fades to a minimal ripple while moving (smooth interpolation).
//
//   2. EYE BLINK
//      A timer-driven scale-squeeze on the eye Static Mesh components.
//      Blinks fire randomly in [BlinkIntervalMin, BlinkIntervalMax] seconds.
//      The blink plays as three phases: Close → Hold → Open, each with
//      independent timing and ease curves for organic feel.
//
// HOW TO ADD TO AkdMyPlayer:
//   • Header  — forward-declare and add UPROPERTY (see integration notes).
//   • Constructor — CreateDefaultSubobject<UkdPlayerHoverComponent>(TEXT("HoverComponent"))
//   • BeginPlay — call HoverComponent->SetMeshComponents(GetMesh(), {EyeLeft, EyeRight})
//
// All tunable values are exposed as EditDefaultsOnly so designers can iterate
// in the BP_Player details panel without touching code.
// ─────────────────────────────────────────────────────────────────────────────

class UStaticMeshComponent;
class USkeletalMeshComponent;
UCLASS(ClassGroup = ("ASKD"), meta = (BlueprintSpawnableComponent))
class THEPERSPVIEW_API UkdPlayerHoverComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UkdPlayerHoverComponent();

	/**
	* Wire up mesh references from AkdMyPlayer::BeginPlay.
	* Must be called before the first tick; safe to call multiple times.
	*
	* @param InBodyMesh   The player's skeletal mesh (GetMesh()).
	* @param InEyeMeshes  Array of eye Static Meshes — both blink in sync.
	*/
	void SetMeshComponents(USkeletalMeshComponent* InBodyMesh, TArray<UStaticMeshComponent*> InEyeMeshes);

    // ─── Hover ───────────────────────────────────────────────────────────────

/** Peak vertical offset (cm) when the player is fully idle. */
    UPROPERTY(EditDefaultsOnly, Category = "Hover", meta = (ClampMin = "0.0", ClampMax = "40.0"))
    float IdleAmplitude = 8.f;

    /** Peak vertical offset (cm) while the player is moving. */
    UPROPERTY(EditDefaultsOnly, Category = "Hover", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float MovingAmplitude = 1.5f;

    /** Oscillation cycles per second (Hz). */
    UPROPERTY(EditDefaultsOnly, Category = "Hover", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float HoverFrequency = 1.2f;

    /**
     * Interpolation speed (alpha/s) when blending amplitude between idle and
     * moving states.  Higher = snappier transitions.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Hover", meta = (ClampMin = "0.5", ClampMax = "15.0"))
    float AmplitudeBlendSpeed = 2.5f;

    /**
     * Horizontal speed (cm/s) below which the player is treated as idle.
     * A small deadzone prevents micro-jitter from killing the full hover.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Hover", meta = (ClampMin = "1.0"))
    float IdleSpeedThreshold = 50.f;

    // ─── Blink ───────────────────────────────────────────────────────────────

    /** Minimum seconds between blink events. */
    UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "0.5"))
    float BlinkIntervalMin = 3.f;

    /** Maximum seconds between blink events. */
    UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "0.5"))
    float BlinkIntervalMax = 8.f;

    /** Seconds for the eye to fully close (fast — mimics natural blink onset) */
    UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "0.01"))
    float BlinkCloseTime = 0.07f;

    /** Seconds the eye stays at peak closure before reopening. */
    UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "0.0"))
    float BlinkHoldTime = 0.04f;

    /** Seconds to reopen — slightly slower than close for organic feel. */
    UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "0.01"))
    float BlinkOpenTime = 0.10f;

    /**
     * Scale factor along the blink axis at full closure.
     * 0 = perfectly flat; 0.05 = a thin sliver (default).
     */
    UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "0.0", ClampMax = "0.5"))
    float BlinkClosedScale = 0.05f;

    /**
     * Which local axis is squeezed during a blink.
     * Default Z = vertical squeeze (eyelid moving down).
     * Change to Y if your eye mesh is oriented differently.
     *   0 = X, 1 = Y, 2 = Z
     */
    UPROPERTY(EditDefaultsOnly, Category = "Blink", meta = (ClampMin = "0", ClampMax = "2"))
    int32 BlinkAxis = 2;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

    // ─── Cached References ────────────────────────────────────────────────────

    UPROPERTY()
    TObjectPtr<USkeletalMeshComponent> BodyMesh;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> EyeMeshes;

    // ─── Hover State ──────────────────────────────────────────────────────────

    /** Accumulated time (radians) that drives Sin(). */
    float HoverPhase = 0.f;

    /** Runtime amplitude, smoothly blended each frame. */
    float CurrentAmplitude = 0.f;

    /**
     * The mesh's original relative location, captured once in BeginPlay.
     * All hover offsets are relative to this so we never drift.
     */
    FVector MeshBaseOffset = FVector::ZeroVector;

    void TickHover(float DeltaTime);

    EBlinkPhase BlinkPhase = EBlinkPhase::Idle;
    float BlinkTimer = 0.f;                                    // elapsed time in the current phase
    FVector EyeBaseScale = FVector(0.30f, 0.30f, 0.30f);      // original scale, captured once

    FTimerHandle BlinkScheduleHandle;

    void TickBlink(float DeltaTime);
    void ScheduleNextBlink();
    void StartBlink();
    void ApplyEyeScale(float AxisValue);            // writes scale to all eye meshes

    UFUNCTION()
    void OnBlinkTimerFired();
		
};
