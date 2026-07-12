// Copyright ASKD Games

#include "Subsystem/kdTutorialSubsystem.h"
#include "Settings/kdTutorialSettings.h"
#include "UI/Widget/kdTutorialPromptWidget.h"
#include "GameInstance/kdGameInstance.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Audio/kdAudioSubsystem.h"
#include "Player/kdPlayerController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputMappingContext.h"


DEFINE_LOG_CATEGORY_STATIC(LogkdTutorial, Log, All);


// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool UkdTutorialSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if (!Super::ShouldCreateSubsystem(Outer)) return false;
    const UWorld* W = Cast<UWorld>(Outer);
    return W && W->IsGameWorld();   // never in editor preview / inactive worlds
}

void UkdTutorialSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);   // mandatory: sets bInitialized for IsTickable
}

void UkdTutorialSubsystem::Deinitialize()
{
    UnbindCompletionWatcher();

    if (IsValid(PromptWidget))
    {
        PromptWidget->RemoveFromParent();   // RemoveFromViewport is deprecated in UE5
    }
    PromptWidget = nullptr;
    Bank = nullptr;
    Queue.Reset();
    bHasActive = false;
    Phase = EPhase::Idle;

    Super::Deinitialize();
}

void UkdTutorialSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    const UkdTutorialSettings* Settings = GetDefault<UkdTutorialSettings>();
    Bank = Settings ? Settings->TutorialBank.LoadSynchronous() : nullptr;

    if (!Bank)
    {
        UE_LOG(LogkdTutorial, Warning,
            TEXT("[kdTutorial] No TutorialBank assigned (Project Settings → Game → kd Tutorial)."));
    }

    if (const UkdGameInstance* GI = InWorld.GetGameInstance<UkdGameInstance>())
    {
        // Honour the accessibility toggle. GameSettings is private, so expose a
        // getter or read it via your existing settings accessor if you have one.
        // bTutorialsEnabled = GI->GetGameSettings().bShowTutorialHints;
    }

    // Default checkpoint = the player's spawn location, captured one tick later so
    // possession has completed. Any tutorial trigger touched afterwards overrides it.
    // (Same next-tick deferral the audio route uses for the possession/loadmap race.)
    //InWorld.GetTimerManager().SetTimerForNextTick(
    //    FTimerDelegate::CreateWeakLambda(this, [this]()
    //        {
    //            if (bHasCheckpoint) return;   // a trigger may already have set one
    //            if (const APawn* P = GetPlayerPawnSafe())
    //            {
    //                CheckpointLocation = P->GetActorLocation();
    //                bHasCheckpoint = true;
    //            }
    //        }));

    // (NEW - UPDATE) Seed default checkpoint AND engage the action lock, one tick after begin play
    // so possession/ASC init has completed (same possession race the audio route hits).
    const FName MapName = FName(*InWorld.GetMapName().Replace(*InWorld.StreamingLevelsPrefix, TEXT("")));

    InWorld.GetTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateWeakLambda(this, [this, MapName]()
            {
                // Default fall-recovery point = spawn (overridden by any trigger touched).
                if (!bHasCheckpoint)
                {
                    if (const APawn* P = GetPlayerPawnSafe())
                    {
                        CheckpointLocation = P->GetActorLocation();
                        bHasCheckpoint = true;
                    }
                }

                // Engage the progressive lock only on designated tutorial maps.
                const UkdTutorialSettings* S = GetDefault<UkdTutorialSettings>();
                if (S && S->ActionLockLevels.Contains(MapName) && S->ActionLockTags.Num() > 0)
                {
                    EngageActionLock(S->ActionLockTags);
                }
            }));
}

UkdTutorialSubsystem* UkdTutorialSubsystem::Get(const UObject* WorldContext)
{
    if (!GEngine || !WorldContext) return nullptr;
    const UWorld* W = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
    return W ? W->GetSubsystem<UkdTutorialSubsystem>() : nullptr;
}

// ─── Tick ─────────────────────────────────────────────────────────────────────

bool UkdTutorialSubsystem::IsTickable() const
{
    // Zero cost when nothing is in flight.
    return IsInitialized() && bTutorialsEnabled && (bHasActive || Queue.Num() > 0);
}

TStatId UkdTutorialSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UkdTutorialSubsystem, STATGROUP_Tickables);
}

void UkdTutorialSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bTutorialsEnabled) return;

    // Death interrupts teaching. Requeue at the head so the lesson survives the retry.
    if (bHasActive && IsPlayerDead())
    {
        const FPending Restore{ Active.StepId, ActiveSource };
        RetireStep(/*bMarkSeen*/ false);
        Queue.Insert(Restore, 0);
        return;
    }

    PhaseTime += DeltaTime;

    switch (Phase)
    {
    case EPhase::Idle:
        if (Queue.Num() > 0)
        {
            const FPending Next = Queue[0];
            Queue.RemoveAt(0);
            BeginStep(Next);
        }
        break;

    case EPhase::Armed:
        EvaluateCondition();
        if (bConditionMet)
        {
            // Solved before we ever spoke. Retire silently — and remember it.
            UE_LOG(LogkdTutorial, Log, TEXT("[kdTutorial] '%s' pre-empted by player. Never shown."),
                *Active.StepId.ToString());
            RetireStep(/*bMarkSeen*/ true);
        }
        else if (PhaseTime >= Active.HintDelaySeconds)
        {
            RevealStep();
        }
        break;

    case EPhase::Showing:
        EvaluateCondition();
        if (bConditionMet && PhaseTime >= Active.MinDisplaySeconds)
        {
            BeginDismiss();
        }
        break;

    case EPhase::Dismissing:
    {
        const float Wait = IsValid(PromptWidget) ? PromptWidget->GetDismissSeconds() : 0.35f;
        if (PhaseTime >= Wait)
        {
            RetireStep(/*bMarkSeen*/ true);
        }
        break;
    }
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

void UkdTutorialSubsystem::RequestStep(FName StepId, AActor* SourceTrigger)
{
    //if (!bTutorialsEnabled || StepId.IsNone() || !Bank) return;

    //if (bHasActive && Active.StepId == StepId) return;
    //if (Queue.ContainsByPredicate([StepId](const FPending& P) { return P.StepId == StepId; })) return;

    //const FkdTutorialStep* Step = Bank->FindStep(StepId);
    //if (!Step)
    //{
    //    UE_LOG(LogkdTutorial, Warning, TEXT("[kdTutorial] Unknown StepId '%s'."), *StepId.ToString());
    //    return;
    //}

    //if (Step->bShowOncePerSave)
    //{
    //    if (const UkdGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UkdGameInstance>() : nullptr)
    //    {
    //        if (GI->HasSeenTutorial(StepId)) return;
    //    }
    //}

    //Queue.Add(FPending{ StepId, SourceTrigger });

    if (!bTutorialsEnabled || StepId.IsNone() || !Bank) return;

    const FkdTutorialStep* Step = Bank->FindStep(StepId);
    if (!Step)
    {
        UE_LOG(LogkdTutorial, Warning, TEXT("[kdTutorial] Unknown StepId '%s'."), *StepId.ToString());
        return;
    }

    // UNLOCK FIRST — unconditionally, regardless of hint state. Touching the box
    // must restore the action even on a replay where the hint is suppressed.
    UnlockAction(Step->UnlockTag);

    if (bHasActive && Active.StepId == StepId) return;
    if (Queue.ContainsByPredicate([StepId](const FPending& P) { return P.StepId == StepId; })) return;

    if (Step->bShowOncePerSave)
    {
        if (const UkdGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UkdGameInstance>() : nullptr)
        {
            if (GI->HasSeenTutorial(StepId)) return;   // hint suppressed — but action already unlocked above
        }
    }

    Queue.Add(FPending{ StepId, SourceTrigger });
}

void UkdTutorialSubsystem::CancelStep(FName StepId)
{
    Queue.RemoveAll([StepId](const FPending& P) { return P.StepId == StepId; });

    if (bHasActive && Active.StepId == StepId)
    {
        RetireStep(/*bMarkSeen*/ false);
    }
}

void UkdTutorialSubsystem::CompleteActiveStep()
{
    if (bHasActive) bConditionMet = true;
}

void UkdTutorialSubsystem::NotifyTriggerExit(AActor* SourceTrigger)
{
    if (!bHasActive || !SourceTrigger) return;
    if (Active.Completion != EkdTutorialCompletion::ExitVolume) return;
    if (ActiveSource.Get() != SourceTrigger) return;

    bConditionMet = true;
}

void UkdTutorialSubsystem::SetTutorialsEnabled(bool bEnabled)
{
    if (bTutorialsEnabled == bEnabled) return;
    bTutorialsEnabled = bEnabled;

    if (!bEnabled)
    {
        Queue.Reset();
        if (bHasActive) RetireStep(/*bMarkSeen*/ false);
        if (IsValid(PromptWidget)) PromptWidget->HideImmediate();
    }
}

void UkdTutorialSubsystem::SetCheckpoint(const FVector& WorldLocation)
{
    CheckpointLocation = WorldLocation;
    bHasCheckpoint = true;
}

bool UkdTutorialSubsystem::GetCheckpoint(FVector& OutLocation) const
{
    if (!bHasCheckpoint) return false;
    OutLocation = CheckpointLocation;
    return true;
}

void UkdTutorialSubsystem::EngageActionLock(const TArray<FGameplayTag>& LockTags)
{
    UAbilitySystemComponent* ASC = GetPlayerASC();
    if (!ASC)
    {
        UE_LOG(LogkdTutorial, Warning, TEXT("[kdTutorial] EngageActionLock: no player ASC yet."));
        return;
    }

    for (const FGameplayTag& Tag : LockTags)
    {
        if (!Tag.IsValid()) continue;

        // Already unlocked (e.g. spawned inside its trigger) → never re-lock it.
        if (UnlockedActions.Contains(Tag)) continue;

        if (!ASC->HasMatchingGameplayTag(Tag))
        {
            ASC->AddLooseGameplayTag(Tag);
        }
    }
    bActionLockEngaged = true;
}

void UkdTutorialSubsystem::UnlockAction(const FGameplayTag& LockTag)
{
    if (!LockTag.IsValid()) return;

    // Record intent even if the ASC isn't ready or the lock hasn't been applied yet.
    // EngageActionLock reads this set, so order between the two next-tick timers
    // no longer matters.
    UnlockedActions.Add(LockTag);

    if (UAbilitySystemComponent* ASC = GetPlayerASC())
    {
        if (ASC->HasMatchingGameplayTag(LockTag))
        {
            ASC->RemoveLooseGameplayTag(LockTag);
        }
    }
}

// ─── Flow ─────────────────────────────────────────────────────────────────────

void UkdTutorialSubsystem::BeginStep(const FPending& Pending)
{
    const FkdTutorialStep* Step = Bank ? Bank->FindStep(Pending.StepId) : nullptr;
    if (!Step) return;

    Active = *Step;            // deep copy — never hold a pointer into the asset
    ActiveSource = Pending.Source;
    bHasActive = true;
    bConditionMet = false;
    Phase = EPhase::Armed;
    PhaseTime = 0.f;

    // Contextual suppression (e.g. don't teach Jump while already crushed).
    if (!Active.SuppressIfAnyTagPresent.IsEmpty())
    {
        if (const UAbilitySystemComponent* ASC = GetPlayerASC())
        {
            if (ASC->HasAnyMatchingGameplayTags(Active.SuppressIfAnyTagPresent))
            {
                RetireStep(/*bMarkSeen*/ false);
                return;
            }
        }
    }

    BindCompletionWatcher();

    // Watcher may already have resolved the condition (tag already in the target state).
    if (bConditionMet)
    {
        RetireStep(/*bMarkSeen*/ true);
        return;
    }

    if (Active.HintDelaySeconds <= 0.f)
    {
        RevealStep();
    }
}

void UkdTutorialSubsystem::RevealStep()
{
    if (!EnsureWidget())
    {
        // No viewport yet (very early frame). Stay Armed and retry next tick.
        return;
    }

    PromptWidget->ShowStep(Active.Headline, Active.Body, ResolveKeyGlyph(Active.PromptAction));

    // All SFX dispatch goes through the audio subsystem. Never PlaySoundAtLocation here.
    if (Active.RevealSound)
    {
        if (UkdAudioSubsystem* Audio = UkdAudioSubsystem::Get(this))
        {
            Audio->PlaySFX2D(Active.RevealSound);
        }
    }

    Phase = EPhase::Showing;
    PhaseTime = 0.f;
}

void UkdTutorialSubsystem::BeginDismiss()
{
    if (IsValid(PromptWidget)) PromptWidget->PlayDismiss();
    Phase = EPhase::Dismissing;
    PhaseTime = 0.f;
}

void UkdTutorialSubsystem::RetireStep(bool bMarkSeen)
{
    UnbindCompletionWatcher();   // MUST run before Active is cleared (needs WatchedTag)

    if (bMarkSeen && Active.bShowOncePerSave && !Active.StepId.IsNone())
    {
        if (UkdGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UkdGameInstance>() : nullptr)
        {
            GI->MarkTutorialSeen(Active.StepId);
        }
    }

    if (IsValid(PromptWidget) && Phase != EPhase::Dismissing)
    {
        PromptWidget->HideImmediate();
    }
    else if (IsValid(PromptWidget))
    {
        PromptWidget->HideImmediate();
    }

    Active = FkdTutorialStep();
    ActiveSource = nullptr;
    bHasActive = false;
    bConditionMet = false;
    Phase = EPhase::Idle;
    PhaseTime = 0.f;
}

void UkdTutorialSubsystem::EvaluateCondition()
{
    if (bConditionMet) return;

    switch (Active.Completion)
    {
    case EkdTutorialCompletion::Timer:
        // Only meaningful once visible — a Timer step must always be seen.
        if (Phase == EPhase::Showing && PhaseTime >= Active.TimerSeconds) bConditionMet = true;
        break;

    case EkdTutorialCompletion::PawnMoved:
        if (const APawn* P = GetPlayerPawnSafe())
        {
            if (P->GetVelocity().SizeSquared() >= FMath::Square(Active.MovedSpeedThreshold))
            {
                bConditionMet = true;
            }
        }
        break;

        // TagAdded / TagRemoved are event-driven; ExitVolume and Manual are push-driven.
    default: break;
    }
}

// ─── Watchers ─────────────────────────────────────────────────────────────────

void UkdTutorialSubsystem::BindCompletionWatcher()
{
    const bool bNeedsTag = (Active.Completion == EkdTutorialCompletion::TagAdded
        || Active.Completion == EkdTutorialCompletion::TagRemoved);
    if (!bNeedsTag || !Active.CompletionTag.IsValid()) return;

    UAbilitySystemComponent* ASC = GetPlayerASC();
    if (!ASC)
    {
        UE_LOG(LogkdTutorial, Warning,
            TEXT("[kdTutorial] '%s' needs a tag watcher but the pawn has no ASC."), *Active.StepId.ToString());
        return;
    }

    WatchedASC = ASC;
    WatchedTag = Active.CompletionTag;
    WatchHandle = ASC->RegisterGameplayTagEvent(WatchedTag, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &UkdTutorialSubsystem::OnWatchedTagChanged);

    // Immediate evaluation — the lesson may already be moot.
    const bool bHas = ASC->HasMatchingGameplayTag(WatchedTag);
    bConditionMet = (Active.Completion == EkdTutorialCompletion::TagAdded) ? bHas : !bHas;
}

void UkdTutorialSubsystem::UnbindCompletionWatcher()
{
    if (WatchedASC.IsValid() && WatchHandle.IsValid() && WatchedTag.IsValid())
    {
        WatchedASC->UnregisterGameplayTagEvent(WatchHandle, WatchedTag, EGameplayTagEventType::NewOrRemoved);
    }
    WatchHandle.Reset();
    WatchedASC = nullptr;
    WatchedTag = FGameplayTag();
}

void UkdTutorialSubsystem::OnWatchedTagChanged(const FGameplayTag ChangedTag, int32 NewCount)
{
    if (!bHasActive) return;

    // Tag ordering caveat: State.Transitioning can still be live when State.CrushMode
    // fires. We only ever read the tag we registered for, so no cross-talk.
    const bool bMet = (Active.Completion == EkdTutorialCompletion::TagAdded)
        ? (NewCount > 0)
        : (NewCount == 0);

    if (bMet) bConditionMet = true;
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

bool UkdTutorialSubsystem::EnsureWidget()
{
    if (IsValid(PromptWidget)) return true;

    UWorld* W = GetWorld();
    APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
    if (!PC || !PC->IsLocalController()) return false;

    const UkdTutorialSettings* Settings = GetDefault<UkdTutorialSettings>();
    if (!Settings) return false;

    // NOTE: TSoftClassPtr resolves the generated class directly. Unlike FClassFinder,
    // no "_C" games — the asset reference already points at the BP class.
    TSubclassOf<UkdTutorialPromptWidget> Cls = Settings->PromptWidgetClass.LoadSynchronous();
    if (!Cls)
    {
        UE_LOG(LogkdTutorial, Warning, TEXT("[kdTutorial] PromptWidgetClass unset."));
        return false;
    }

    // CreateWidget + AddToViewport, never UWidgetComponent: the instance is stable
    // across visibility changes, so bound delegates and animation state survive.
    PromptWidget = CreateWidget<UkdTutorialPromptWidget>(PC, Cls);
    if (!IsValid(PromptWidget)) return false;

    PromptWidget->AddToViewport(Settings->PromptZOrder);
    PromptWidget->HideImmediate();
    return true;
}

APawn* UkdTutorialSubsystem::GetPlayerPawnSafe() const
{
    return UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
}

UAbilitySystemComponent* UkdTutorialSubsystem::GetPlayerASC() const
{
    return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPlayerPawnSafe());
}

bool UkdTutorialSubsystem::IsPlayerDead() const
{
    const UAbilitySystemComponent* ASC = GetPlayerASC();
    return ASC && ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_Dead);
}

FText UkdTutorialSubsystem::ResolveKeyGlyph(const UInputAction* Action) const
{
    if (!Action || !GetWorld()) return FText::GetEmpty();

    const AkdPlayerController* PC = Cast<AkdPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC || !PC->InputMappingContext) return FText::GetEmpty();

    // Read the live mapping context so a rebind is reflected in the copy for free.
    FKey GamepadFallback;
    for (const FEnhancedActionKeyMapping& M : PC->InputMappingContext->GetMappings())
    {
        if (M.Action != Action) continue;

        if (!M.Key.IsGamepadKey())
        {
            return M.Key.GetDisplayName(/*bLongDisplayName*/ false);   // "LMB", "Space"
        }
        if (!GamepadFallback.IsValid()) GamepadFallback = M.Key;
    }

    return GamepadFallback.IsValid()
        ? GamepadFallback.GetDisplayName(false)
        : FText::GetEmpty();
}