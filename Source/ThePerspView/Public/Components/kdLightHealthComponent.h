// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "kdLightHealthComponent.generated.h"


class AkdMyPlayer;
class UkdAttributeSet;
class UkdDeathComponent;
struct FGameplayTag;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEPERSPVIEW_API UkdLightHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UkdLightHealthComponent();

    // ── Config ────────────────────────────────────────────────────────────────

   /** Health points lost per second while the player is exposed to light. */
    UPROPERTY(EditDefaultsOnly, Category = "Light Health", meta = (ClampMin = "0.1"))
    float LightDamageRate = 10.f;

    /** Health points recovered per second while the player stands in shadow. */
    UPROPERTY(EditDefaultsOnly, Category = "Light Health", meta = (ClampMin = "0.1"))
    float ShadowHealRate = 5.f;

    /** Tick interval (seconds) for the health update timer. Lower = smoother bar. */
    UPROPERTY(EditDefaultsOnly, Category = "Light Health", meta = (ClampMin = "0.02", ClampMax = "0.5"))
    float HealthTickInterval = 0.1f;

    /** Fraction of MaxLightHealth below which the "critical" state is entered. */
    UPROPERTY(EditDefaultsOnly, Category = "Light Health", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CriticalHealthThreshold = 0.25f;

    // ── State Query ───────────────────────────────────────────────────────────

    /** Returns true if current health is below CriticalHealthThreshold. */
    UFUNCTION(BlueprintPure, Category = "Light Health")
    bool IsCritical() const;

    /** Returns health in the range [0, 1]. */
    UFUNCTION(BlueprintPure, Category = "Light Health")
    float GetHealthPercent() const;

    /** Returns true if the player is currently standing in a shadow volume
    *  (healing). Used by UkdLightHealthWidget to seed the light-exposure
    *  warning sign's initial visibility before the first
    *  OnShadowStateChanged broadcast. */
    UFUNCTION(BlueprintPure, Category = "Light Health")
    bool IsInShadow() const { return bIsInShadow; }

    // ── Delegates (widget binds here) ─────────────────────────────────────────

    /** Fires every tick with the new current and max values. */
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLightHealthChanged, float /*Current*/, float /*Max*/);
    FOnLightHealthChanged OnLightHealthChanged;

    /** Fires when the critical state transitions on or off. */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnLightCriticalChanged, bool /*bIsCritical*/);
    FOnLightCriticalChanged OnLightCriticalChanged;

    /** Fires when the shadow/light exposure state changes (for widget colour). */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnShadowStateChanged, bool /*bIsInShadow*/);
    FOnShadowStateChanged OnShadowStateChanged;

    // ── External API ──────────────────────────────────────────────────────────

    /** Fully restores LightHealth. Called automatically on respawn via delegate. */
    void RestoreLightHealth();

    /** Permanently halts the drain/heal loop and the danger-sign flash for the
    *  rest of the level (e.g. on level complete). Idempotent. */
    UFUNCTION(BlueprintCallable, Category = "Light Health")
    void Freeze();

protected:	
	virtual void BeginPlay() override;	

private:
    // ── Cached references ─────────────────────────────────────────────────────

    UPROPERTY()
    TObjectPtr<AkdMyPlayer> CachedPlayer;

    // ── Health tick ───────────────────────────────────────────────────────────

    FTimerHandle HealthTickHandle;
    bool bIsInShadow = false;
    bool bWasCritical = false;
    bool bFrozen = false;

    void StartHealthTick();
    void StopHealthTick();
    void HealthTick();

    // ── Attribute helpers ─────────────────────────────────────────────────────

    float GetCurrentHealth() const;
    float GetMaxHealth() const;
    void  SetCurrentHealth(float NewValue);

    // ── Tag callbacks (registered on ASC) ────────────────────────────────────

    void OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount);
    void OnInShadowTagChanged(const FGameplayTag Tag, int32 NewCount);

    // ── Respawn callback ──────────────────────────────────────────────────────

    UFUNCTION()
    void OnRespawnComplete();

    // ── Death ─────────────────────────────────────────────────────────────────

    void TriggerLightDeath();
};
