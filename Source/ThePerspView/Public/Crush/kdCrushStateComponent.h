// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "kdCrushStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDrainStateChanged, bool, bIsDraining);

// ─────────────────────────────────────────────────────────────────────────────
// EkdLightSourceType — disambiguates how each registered light's shadow
// direction is derived during the per-check trace.
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EkdLightSourceType : uint8
{
	Directional UMETA(DisplayName = "Directional"),  // infinite range, constant direction
	Point       UMETA(DisplayName = "Point"),         // local, radial
	Spot        UMETA(DisplayName = "Spot"),          // local, cone-limited
	Rect        UMETA(DisplayName = "Rect"),          // local, radial (treated like Point)
};

// ─────────────────────────────────────────────────────────────────────────────
// FkdRegisteredLight — lightweight runtime entry for each discovered light.
// Caches the data needed to cull and trace without re-querying the component.
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT()
struct FkdRegisteredLight
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> LightActor = nullptr;

	EkdLightSourceType Type = EkdLightSourceType::Directional;

	/** Cached attenuation radius (cm). 0 = infinite (directional). */
	float AttenuationRadius = 0.f;

	/** Cached outer half-cone angle (degrees). Spot lights only. */
	float OuterConeAngleDeg = 0.f;
};

//class ADirectionalLight;
class UCharacterMovementComponent;
class AkdMyPlayer;
class UGameplayEffect;
struct FGameplayTag;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEPERSPVIEW_API UkdCrushStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UkdCrushStateComponent();

	// Shadow tracking and movements settings /////////////////////////////////
	UFUNCTION(BlueprintCallable, Category = "Crush | Physics")
	void ToggleShadowTracking(bool bEnable);

	void HandleVerticalInput(float Value);

	UFUNCTION(BlueprintPure, Category = "Crush | Physics")
	bool IsStandingInShadow() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowCheckFrequency = 0.05f;				// default 20 Hz

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowCheckFrequencyMoving = 0.016f;		// higher frequency when moving

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowJumpPower = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowBrakingDeceleration = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	bool bShowDebugLines = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowTraceDistance = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush | Settings")
	float ShadowMoveSpeed = 300.0f;
	//////////////////////////////////////////////////////////////////////////

	// ── Multi-Light Settings ──────────────────────────────────────────────────
	/**
	 * Controls how multiple lights vote on the InShadow state:
	 *   true  → ALL active lights that reach the player must be blocked (stricter,
	 *            harder to hide — recommended for tense gameplay).
	 *   false → ANY single blocked light qualifies as shadow (more forgiving).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Crush | Lights")
	bool bRequireAllLightsBlocked = true;

	/**
	 * Optional manual list. When populated, ONLY these actors are used as
	 * light sources and auto-discovery is skipped entirely.
	 * Ideal for tightly authored levels requiring exact control.
	 * Supports any mix of ADirectionalLight, APointLight, ASpotLight, ARectLight.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Crush | Lights")
	TArray<TObjectPtr<AActor>> ManualLightOverrides;

	// Stamina handling drain / regen settings ///////////////////////////////
	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	float BaseStaminaDrainRate = 5.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	float ShadowStaminaDrainRate = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	TSubclassOf<UGameplayEffect> StaminaModEffectClass;

	UPROPERTY(BlueprintAssignable, Category = "Stamina")
	FOnDrainStateChanged OnDrainStateChanged;

	// ── Stamina Regen (3D only, delayed) ────────────────────────────────────────

	/** How long after exiting Crush Mode before regen begins (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	float RegenDelay = 2.0f;

	/** Stamina points restored per second during regen. */
	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	float StaminaRegenRate = 5.0f;

	/** Timer firing frequency for regen ticks. Lower = smoother, more GAS calls. */
	UPROPERTY(EditDefaultsOnly, Category = "Stamina")
	float RegenTickInterval = 0.1f;
	//////////////////////////////////////////////////////////////////////////

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void FindDirectionalLight();
	void ResetPhysicsTo3D();
	void DiscoverAndRegisterLights();

private:
	//UPROPERTY(EditDefaultsOnly, Category = "Crush | Lights")
	//TObjectPtr<ADirectionalLight> DirectionalLightActor;

	// ── Runtime light registry ────────────────────────────────────────────────

	UPROPERTY()
	TArray<FkdRegisteredLight> RegisteredLights;

	/**
	 * Attempts to build a FkdRegisteredLight from a world actor.
	 * Returns false if the actor is not a supported light type or is invisible.
	 * Check order: Directional → Spot → Point/Rect  (Spot must precede Point
	 * because USpotLightComponent inherits UPointLightComponent in UE5).
	 */
	bool TryBuildLightEntry(AActor* Actor, FkdRegisteredLight& OutEntry) const;

	/**
	 * Computes the world-space direction from PlayerLocation toward the given
	 * registered light. Returns FVector::ZeroVector when the light does not
	 * affect the player (outside attenuation radius or spot cone).
	 * ZeroVector is used as the "skip this light" sentinel in IsStandingInShadow.
	 */
	FVector GetLightDirectionForPlayer(const FkdRegisteredLight& Light,
		const FVector& PlayerLocation) const;

	// ── Internals ─────────────────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, Category = "Crush")
	float CrushGravityScale = 0.25f;

	UPROPERTY()
	TObjectPtr<AkdMyPlayer> CachedOwner = nullptr;

	UFUNCTION()
	void ApplyStaminaDelta(float Delta);	// Positive for regen, negative for drain

	void OnCrushModeTagChanged_Regen(const FGameplayTag Tag, int32 NewCount);
	void BeginRegen();
	void RegenTick();
	void StopRegen();

	FTimerHandle RegenDelayHandle;
	FTimerHandle RegenTickHandle;
	bool bIsRegenerating = false;
	float TimeSinceLastShadowCheck = 0.0f;
	float TimeSinceLastMove = 0.0f;
	//FVector CachedLightDirection;
	bool bIsInShadow = false;
	bool bWasDraining = false;
	bool bWasMoving = false;
};
