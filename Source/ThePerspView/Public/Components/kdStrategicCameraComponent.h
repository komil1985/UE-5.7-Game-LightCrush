// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdStrategicCameraComponent.generated.h"


DECLARE_MULTICAST_DELEGATE_OneParam(FkdSurveyAlphaSignature, float /*Alpha 0..1*/);

class USpringArmComponent;
class UCameraComponent;
class UCurveFloat;

/**
 * UkdStrategicCameraComponent
 * ---------------------------------------------------------------------------
 * "Surveyor's Plate" strategic overview for ThePerspView's 3D mode.
 *
 * Press-and-hold engages a smooth dolly-back of the telephoto rig so the player
 * can survey the whole level for strategic / puzzle planning; releasing returns
 * the camera to its exact gameplay baseline.
 *
 * DESIGN
 *  - Dolly-back (TargetArmLength * multiplier) + a small FOV widen are applied
 *    TOGETHER. Doing both cancels the classic "dolly-zoom" vertigo warp, so the
 *    level reads as geometrically honest while you survey it.
 *  - Optional survey pitch tilts the rig downward for an RTS-style read of the
 *    floor plan. It is auto-gated: it only applies when the spring arm is NOT
 *    driven by pawn control rotation, so it can never fight your rotation source.
 *
 * ARCHITECTURE
 *  - This component is the single owner of the strategic camera lerp. It caches
 *    the LIVE 3D baseline the moment it first engages from rest, and restores
 *    those exact values on full return, so it adapts to whatever the current
 *    gameplay camera state is rather than baking constants.
 *  - It is fully decoupled from GAS, the CMC, the crush / geometry-transition
 *    pipeline, and UkdWorldColorDriver. It writes ONLY to the spring arm and
 *    camera it owns. It performs no MPC / post-process writes, honoring the
 *    single-writer rule (UkdWorldColorDriver remains the sole PP/MPC owner).
 *    See the optional survey-tint hook in the .cpp if you want a cyanotype wash;
 *    that must route through the driver, never directly to the MPC.
 *  - Tick is enabled only while interpolating and self-disables once settled,
 *    so the component costs nothing at steady state.
 *
 * USAGE
 *  - Add to your 3D player pawn. It auto-resolves the owner's spring arm and
 *    camera in BeginPlay, or call Initialize() to wire them explicitly.
 *  - Drive it from UkdGA_StrategicView (recommended, gives tag-gating) or from
 *    any input handler via RequestStrategicView(true/false).
 */
UCLASS(ClassGroup = (kd), meta = (BlueprintSpawnableComponent), DisplayName = "kd Strategic Camera")
class THEPERSPVIEW_API UkdStrategicCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UkdStrategicCameraComponent();

	//~ Begin UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent interface

	/**
	 * Explicitly bind the rig this component drives. Optional: if not called,
	 * BeginPlay resolves the owner's first spring arm + camera automatically.
	 */
	UFUNCTION(BlueprintCallable, Category = "kd|Strategic Camera")
	void Initialize(USpringArmComponent* InSpringArm, UCameraComponent* InCamera);

	/**
	 * Engage (true) or release (false) the strategic overview. The transition is
	 * interpolated over EngageTime / ReturnTime; calling repeatedly is safe and
	 * simply re-targets the lerp.
	 */
	UFUNCTION(BlueprintCallable, Category = "kd|Strategic Camera")
	void RequestStrategicView(bool bEngage);

	/**
	 * Snap back to the cached gameplay baseline instantly (no lerp) and clear all
	 * runtime state. Call this from a hard mode-switch (e.g. crush entry) if you
	 * ever need to guarantee an immediate reset rather than a graceful return.
	 */
	UFUNCTION(BlueprintCallable, Category = "kd|Strategic Camera")
	void ForceReturnImmediate();

	/** True while engaged or still interpolating toward the strategic state. */
	UFUNCTION(BlueprintPure, Category = "kd|Strategic Camera")
	bool IsEngaged() const { return bEngaged; }

	/** Normalized transition state [0 = gameplay baseline, 1 = full survey]. */
	UFUNCTION(BlueprintPure, Category = "kd|Strategic Camera")
	float GetSurveyAlpha() const { return CurrentAlpha; }

	/** Fires whenever the survey alpha changes (incl. settle to 0). The world-color
	 *  driver binds to this to ramp the tilt-shift band. Cosmetic only. */
	FkdSurveyAlphaSignature OnSurveyAlphaChanged;

protected:
	// ---- Tunables ----------------------------------------------------------

	/** Strategic arm length as a multiple of the cached gameplay arm length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "kd|Strategic Camera|Framing", meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "8.0"))
	float StrategicArmMultiplier = 3.0f;

	/** Additive FOV widen (degrees) at full survey. Paired with the dolly-back to cancel dolly-zoom warp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "kd|Strategic Camera|Framing", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "60.0"))
	float StrategicFOVDelta = 18.0f;

	/** Additive pitch (degrees) at full survey; negative tilts the rig downward for a top-down read. Auto-gated off when the arm uses pawn control rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "kd|Strategic Camera|Framing", meta = (UIMin = "-60.0", UIMax = "0.0"))
	float SurveyPitchDelta = -18.0f;

	/** Apply the survey pitch (only honored when the spring arm is not driven by pawn control rotation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "kd|Strategic Camera|Framing")
	bool bApplySurveyPitch = true;

	/** Seconds to lerp from baseline to full survey on engage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "kd|Strategic Camera|Timing", meta = (ClampMin = "0.01", UIMin = "0.05", UIMax = "2.0"))
	float EngageTime = 0.45f;

	/** Seconds to lerp back to baseline on release (slightly snappier than engage feels responsive). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "kd|Strategic Camera|Timing", meta = (ClampMin = "0.01", UIMin = "0.05", UIMax = "2.0"))
	float ReturnTime = 0.32f;

	/** Optional easing curve evaluated on the raw alpha. If null, a smoothstep ease is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "kd|Strategic Camera|Timing")
	TObjectPtr<UCurveFloat> EaseCurve = nullptr;

	/** Disable spring-arm collision while surveying so the dolly-back is not clipped short by walls behind the player. Restored on full return. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "kd|Strategic Camera|Behavior")
	bool bDisableCollisionDuringView = true;

private:
	// ---- Wiring ------------------------------------------------------------

	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> SpringArm = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera = nullptr;

	// ---- Runtime state -----------------------------------------------------

	/** Cached gameplay baseline, captured live the first time we engage from rest. */
	float BaseArmLength = 0.0f;
	float BaseFOV = 0.0f;
	float BasePitch = 0.0f;
	bool bBaseCollisionTest = true;

	/** Whether the survey pitch may be applied this engagement (decided at cache time). */
	bool bCanApplySurveyPitch = false;

	/** True once BaseXxx hold a valid captured baseline awaiting restore. */
	bool bBaseCached = false;

	/** Target direction: true = move toward survey, false = move toward baseline. */
	bool bEngaged = false;

	/** Normalized transition [0,1]. */
	float CurrentAlpha = 0.0f;

	// ---- Internal ----------------------------------------------------------

	bool ResolveRig();
	void CacheBaseState();
	void RestoreBaseState();
	void ApplyAlpha(float Eased);
};
