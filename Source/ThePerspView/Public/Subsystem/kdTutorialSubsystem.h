// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/kdTutorialBank.h"
#include "kdTutorialSubsystem.generated.h"

class UkdTutorialPromptWidget;
class UAbilitySystemComponent;
class UInputAction;
class APawn;

/**
 * UkdTutorialSubsystem — the single owner of tutorial state and the prompt widget.
 *
 * OWNERSHIP CONTRACT (same discipline as UkdWorldColorDriver):
 *   • This subsystem is the ONLY thing that creates, shows, or hides the prompt widget.
 *   • It never writes post-process, never touches the vitals HUD, never plays sound
 *     directly — reveal SFX go through UkdAudioSubsystem::PlaySFX2D.
 *   • Triggers and abilities only ever call RequestStep / CancelStep / CompleteActiveStep.
 *
 * LIFECYCLE OF A STEP
 *   Idle → (queued) → Armed → Showing → Dismissing → Idle
 *
 *   Armed  : completion watcher is LIVE but nothing is on screen. If the player
 *            satisfies the condition here, the step is retired silently and marked
 *            seen. This is the adaptive-silence guarantee.
 *   Showing: plate is up. Dismisses when condition met AND MinDisplaySeconds elapsed.
 */
UCLASS()
class THEPERSPVIEW_API UkdTutorialSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    // ── Lifecycle ─────────────────────────────────────────────────────────────
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

    // ── FTickableGameObject ───────────────────────────────────────────────────
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override;
    virtual TStatId GetStatId() const override;

    /** Convenience accessor: UkdTutorialSubsystem::Get(this)->RequestStep(...) */
    static UkdTutorialSubsystem* Get(const UObject* WorldContext);

    // ── Public API ────────────────────────────────────────────────────────────

    /** Queue a lesson. No-op if unknown, already queued/active, or already learned. */
    UFUNCTION(BlueprintCallable, Category = "kd|Tutorial")
    void RequestStep(FName StepId, AActor* SourceTrigger = nullptr);

    /** Remove a step whether queued or active. Does not mark it seen. */
    UFUNCTION(BlueprintCallable, Category = "kd|Tutorial")
    void CancelStep(FName StepId);

    /** Satisfies the active step's condition. The only way to finish a Manual step. */
    UFUNCTION(BlueprintCallable, Category = "kd|Tutorial")
    void CompleteActiveStep();

    /** Called by AkdTutorialTrigger on EndOverlap. Drives ExitVolume completion. */
    void NotifyTriggerExit(AActor* SourceTrigger);

    /** Bound to the accessibility setting. Disabling clears everything in flight. */
    UFUNCTION(BlueprintCallable, Category = "kd|Tutorial")
    void SetTutorialsEnabled(bool bEnabled);

    /** Called by AkdTutorialTrigger on entry. Becomes the fall-recovery point. */
    void SetCheckpoint(const FVector& WorldLocation);

    /** Latest checkpoint. Returns false only if nothing has ever been set. */
    bool GetCheckpoint(FVector& OutLocation) const;

private:
    enum class EPhase : uint8 { Idle, Armed, Showing, Dismissing };

    struct FPending
    {
        FName StepId = NAME_None;
        TWeakObjectPtr<AActor> Source;
    };

    // ── Flow ──────────────────────────────────────────────────────────────────
    void BeginStep(const FPending& Pending);
    void RevealStep();
    void BeginDismiss();
    void RetireStep(bool bMarkSeen);
    void EvaluateCondition();

    // ── Watchers ──────────────────────────────────────────────────────────────
    void BindCompletionWatcher();
    void UnbindCompletionWatcher();
    void OnWatchedTagChanged(const FGameplayTag ChangedTag, int32 NewCount);

    // ── Helpers ───────────────────────────────────────────────────────────────
    bool  EnsureWidget();
    APawn* GetPlayerPawnSafe() const;
    UAbilitySystemComponent* GetPlayerASC() const;
    FText ResolveKeyGlyph(const UInputAction* Action) const;
    bool  IsPlayerDead() const;

    // ── State ─────────────────────────────────────────────────────────────────
    UPROPERTY() TObjectPtr<UkdTutorialBank> Bank = nullptr;
    UPROPERTY() TObjectPtr<UkdTutorialPromptWidget> PromptWidget = nullptr;

    // ── Fall-recovery checkpoint ────────────────────────────────────────────
    FVector CheckpointLocation = FVector::ZeroVector;
    bool    bHasCheckpoint = false;

    TArray<FPending> Queue;

    /** A COPY, not a pointer into Bank->Steps — immune to asset reload/reallocation. */
    FkdTutorialStep Active;
    TWeakObjectPtr<AActor> ActiveSource;

    EPhase Phase = EPhase::Idle;
    float  PhaseTime = 0.f;
    bool   bHasActive = false;
    bool   bConditionMet = false;
    bool   bTutorialsEnabled = true;

    TWeakObjectPtr<UAbilitySystemComponent> WatchedASC;
    FDelegateHandle WatchHandle;
    FGameplayTag    WatchedTag;   // cached so Unregister always matches Register
};