// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdDeathComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// EPlayerDeathState — explicit state machine so every system can query the
// exact phase the player is in without relying on tag combinations.
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EPlayerDeathState : uint8
{
	Alive       UMETA(DisplayName = "Alive"),
	Dying       UMETA(DisplayName = "Dying"),       // death sequence playing
	Dead        UMETA(DisplayName = "Dead"),         // waiting for GM decision
	Respawning  UMETA(DisplayName = "Respawning"),  // fading back in
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeathSequenceComplete);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRespawnComplete);

class AkdMyPlayer;
class APlayerCameraManager;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEPERSPVIEW_API UkdDeathComponent : public UActorComponent
{
	GENERATED_BODY()

public:
    UkdDeathComponent();

    // ── State Query ───────────────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "Death")
    EPlayerDeathState GetDeathState() const { return DeathState; }

    UFUNCTION(BlueprintPure, Category = "Death")
    bool IsAlive() const { return DeathState == EPlayerDeathState::Alive; }

    // ── External API (called by GameMode) ────────────────────────────────────

    /**
     * Begins the full death sequence:
     *   → sets state to Dying
     *   → disables input
     *   → triggers camera red-flash fade
     *   → starts slow-motion dip
     *   → fires OnDeathSequenceComplete after DeathFadeDuration seconds
     *      so GameMode can decide lives / game-over.
     */
    UFUNCTION(BlueprintCallable, Category = "Death")
    void TriggerDeath();

    /**
     * Teleports the player, restores GAS state, and plays the respawn
     * fade-in.  Always safe to call even if TriggerDeath was never called
     * (e.g. forced respawn from debug console).
     */
    UFUNCTION(BlueprintCallable, Category = "Death")
    void TriggerRespawn(const FVector& Location, const FRotator& Rotation);

    // ── Delegates ─────────────────────────────────────────────────────────────

    /** Fires when the death animation/fade is done. GameMode listens here. */
    UPROPERTY(BlueprintAssignable, Category = "Death")
    FOnDeathSequenceComplete OnDeathSequenceComplete;

    /** Fires when the respawn fade-in finishes and the player is controllable. */
    UPROPERTY(BlueprintAssignable, Category = "Death")
    FOnRespawnComplete OnRespawnComplete;

    // ── Blueprint hooks ───────────────────────────────────────────────────────

    /** Override in Blueprint to play a death montage, particles, etc. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Death")
    void BP_OnDeathStarted();

    /** Override in Blueprint for respawn VFX (sparkle, materialisation, etc.). */
    UFUNCTION(BlueprintImplementableEvent, Category = "Death")
    void BP_OnRespawnStarted();

    // ── Config ────────────────────────────────────────────────────────────────

    /**
     * Duration of the red-fade-to-black during death.
     * Death sequence complete fires after this elapses.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Death | Timing", meta = (ClampMin = "0.2"))
    float DeathFadeDuration = 1.2f;

    /** How fast the world slows when the player dies (0 = instant freeze). */
    UPROPERTY(EditDefaultsOnly, Category = "Death | Timing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DeathSlowMotionScale = 0.25f;

    /** Duration of the fade-in on respawn. */
    UPROPERTY(EditDefaultsOnly, Category = "Death | Timing", meta = (ClampMin = "0.1"))
    float RespawnFadeDuration = 0.6f;

    /** Stamina percentage restored to the player on respawn (0–1). */
    UPROPERTY(EditDefaultsOnly, Category = "Death | Balance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RespawnStaminaPercent = 1.0f;

    /** Color tint of the death screen fade. */
    UPROPERTY(EditDefaultsOnly, Category = "Death | Visuals")
    FLinearColor DeathFadeColor = FLinearColor(0.6f, 0.0f, 0.0f, 1.0f);  // dark red

protected:
    virtual void BeginPlay() override;

private:
    EPlayerDeathState DeathState = EPlayerDeathState::Alive;

    FTimerHandle DeathSequenceTimerHandle;
    FTimerHandle SlowMotionRestoreHandle;
    FTimerHandle RespawnCompleteHandle;

    // Cache expensive casts done once in BeginPlay
    UPROPERTY()
    TObjectPtr<AkdMyPlayer> CachedPlayer;

    APlayerCameraManager* GetCameraManager() const;

    void FinishDeathSequence();
    void FinishRespawnFade();

    void SetDeathState(EPlayerDeathState NewState);
    void DisablePlayerInput();
    void EnablePlayerInput();
    void ClearAllGASDeathState();
    void RestoreStamina();
		
};


// ─────────────────────────────────────────────────────────────────────────────
// UkdDeathComponent — attached to AkdMyPlayer.
//
// RESPONSIBILITIES:
//   • Own EPlayerDeathState — single source of truth for death phase.
//   • Play the death visual sequence:
//       1. Red screen-flash fade  (CameraManager::StartCameraFade)
//       2. Slow-motion dip        (SetGlobalTimeDilation → 0.3 → 1.0)
//       3. Mesh hide              (deferred, after fade completes)
//   • Disable / re-enable player input atomically.
//   • Restore GAS state (tags + stamina) on respawn.
//   • Broadcast delegates so GameMode / HUD stay decoupled.
//
// SETUP:
//   1. Add to AkdMyPlayer in the constructor (CreateDefaultSubobject).
//   2. Call DeathComponent->TriggerDeath() from AkdGameModeBase::HandlePlayerDeath.
//   3. Call DeathComponent->TriggerRespawn(Location, Rotation) from
//      AkdGameModeBase::PerformRespawn.
//   4. Tune DeathFadeDuration / RespawnFadeDuration in the BP subclass Details.
//
// DEPENDENCIES:
//   • AkdMyPlayer must implement IAbilitySystemInterface.
//   • FkdGameplayTags must be initialized before BeginPlay.
// ─────────────────────────────────────────────────────────────────────────────