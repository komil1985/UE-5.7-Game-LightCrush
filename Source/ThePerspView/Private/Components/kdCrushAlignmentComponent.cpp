// Copyright ASKD Games


#include "Components/kdCrushAlignmentComponent.h"
#include "Player/kdMyPlayer.h"
#include "Crush/kdCrushDirectionLibrary.h"
#include "GameplayTags/kdGameplayTags.h"
#include "AbilitySystem/Abilities/kd_CrushToggle.h"
#include "Components/kdGameFeedbackComponent.h"
#include "Audio/kdAudioSubsystem.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"


namespace
{
    /**
     * Signed shortest-arc error from Yaw to the nearest 90-degree cardinal.
     *
     * The RoundToInt / & 3 fold is the same one UkdCrushDirectionLibrary uses;
     * it is repeated here only because the library exposes the ABSOLUTE error
     * and the assist needs the SIGN to know which way to turn. Keeping the two
     * in lockstep matters: if the library's cardinal convention ever changes,
     * this must change with it.
     *
     * @return Degrees to ADD to Yaw to land exactly on the cardinal.
     */
    FORCEINLINE float SignedErrorToNearestCardinal(float Yaw, float& OutSnappedYaw)
    {
        const int32 CardinalIndex = FMath::RoundToInt(Yaw / 90.f) & 3;
        OutSnappedYaw = static_cast<float>(CardinalIndex) * 90.f;
        return FMath::FindDeltaAngleDegrees(Yaw, OutSnappedYaw);
    }
}

// =============================================================================
// Construction / lifetime
// =============================================================================

UkdCrushAlignmentComponent::UkdCrushAlignmentComponent()
{
    PrimaryComponentTick.bCanEverTick = true;

    // TG_PrePhysics + a prerequisite on the PlayerController (installed in
    // CacheOwners) means we run after UpdateRotation has consumed this frame's
    // look input, so PlayerYawRateDegPerSec measures real player intent.
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UkdCrushAlignmentComponent::BeginPlay()
{
    Super::BeginPlay();

    CacheOwners();

    // Two deferrals are required, for two different races:
    //   1. Possession — GetController() can still be null at component BeginPlay.
    //   2. Ability grant — AkdMyPlayer::BeginPlay grants DefaultAbilities AFTER
    //      Super::BeginPlay dispatches component BeginPlay, so the CDO read
    //      cannot succeed yet.
    // Next-tick resolves both; TickComponent retries if it somehow does not.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateWeakLambda(this, [this]()
                {
                    CacheOwners();

                    if (!bToleranceResolved)
                    {
                        ResolvedToleranceDegrees = ResolveToleranceFromAbilityCDO();
                    }

#if !UE_BUILD_SHIPPING
                    UE_LOG(LogTemp, Log,
                        TEXT("[Register] Init | tolerance=%.1f deg (resolved=%d) | capture arc=%.1f deg "
                            "| assist=%d | buffer=%d"),
                        ResolvedToleranceDegrees, bToleranceResolved ? 1 : 0,
                        GetCaptureArcDegrees(),
                        bYawAssistEnabled ? 1 : 0, bIntentBufferEnabled ? 1 : 0);
#endif
                }));
    }
}

void UkdCrushAlignmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Never leave a latched press behind on level transition / destroy — the
    // callback target is about to be torn down.
    CancelPendingIntent(/*bBroadcastFailure*/ false);

    if (CachedPC)
    {
        PrimaryComponentTick.RemovePrerequisite(CachedPC, CachedPC->PrimaryActorTick);
    }

    CachedPlayer = nullptr;
    CachedPC = nullptr;
    CachedASC = nullptr;

    Super::EndPlay(EndPlayReason);
}

void UkdCrushAlignmentComponent::CacheOwners()
{
    if (!CachedPlayer)
    {
        CachedPlayer = Cast<AkdMyPlayer>(GetOwner());
    }
    if (!CachedPlayer)
    {
        return;
    }

    if (!CachedASC)
    {
        CachedASC = CachedPlayer->GetAbilitySystemComponent();
    }

    if (!CachedPC)
    {
        if (APlayerController* PC = Cast<APlayerController>(CachedPlayer->GetController()))
        {
            CachedPC = PC;

            // Guarantees this component ticks AFTER the controller has applied
            // AddYawInput for the frame. Without it, our yaw write can be
            // overwritten by UpdateRotation in the same frame and the assist
            // silently does nothing on roughly half the frames.
            PrimaryComponentTick.AddPrerequisite(PC, PC->PrimaryActorTick);
        }
    }
}

// =============================================================================
// Tick — the whole system in one readable pass
// =============================================================================

void UkdCrushAlignmentComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedPlayer || !CachedPC || !CachedASC)
    {
        CacheOwners();
        if (!CachedPlayer || !CachedPC || !CachedASC)
        {
            return;
        }
    }

    if (!bToleranceResolved)
    {
        ResolvedToleranceDegrees = ResolveToleranceFromAbilityCDO();
    }

    const float CurrentYaw = FRotator::ClampAxis(CachedPC->GetControlRotation().Yaw);

    // ── Isolate the player's own input from our assist ──────────────────────
    // Comparing against LastWrittenYaw (not last frame's raw yaw) is the whole
    // trick: the difference is exactly what the player contributed since we let
    // go of the wheel. Measuring raw frame-to-frame delta instead would feed the
    // assist's own output back into the speed gate and make it oscillate.
    PlayerYawRateDegPerSec =
        (bHasWrittenYaw && DeltaTime > KINDA_SMALL_NUMBER)
        ? FMath::Abs(FMath::FindDeltaAngleDegrees(LastWrittenYaw, CurrentYaw)) / DeltaTime
        : 0.f;

    // ── Availability gate — we write NOTHING while another system owns the camera
    if (!IsRegisterAvailable())
    {
        CancelPendingIntent(/*bBroadcastFailure*/ true);
        ResetRegister();
        bHasWrittenYaw = false;   // next available frame starts a clean measurement
        return;
    }

    EvaluateRegister(CurrentYaw);

    float SnappedYaw = 0.f;
    const float SignedError = SignedErrorToNearestCardinal(CurrentYaw, SnappedYaw);

    // ═══════════════════════════════════════════════════════════════════════
    // LAYER 2 — a latched press owns the camera outright while it is live
    // ═══════════════════════════════════════════════════════════════════════
    if (bIntentPending)
    {
        IntentTimeRemaining -= DeltaTime;

        if (AlignmentErrorDegrees <= ResolvedToleranceDegrees)
        {
            // Land on the cardinal EXACTLY before firing. The ability re-resolves
            // the direction from control yaw, so a zero-error hand-off also means
            // the crush camera and collapse plane carry no residual yaw.
            ApplyYaw(SnappedYaw);
            FireBufferedIntent();
            return;
        }

        const bool bPlayerOverrode = (PlayerYawRateDegPerSec > IntentCancelYawRate);
        const bool bTimedOut = (IntentTimeRemaining <= 0.f);

        if (bPlayerOverrode || bTimedOut)
        {
            // A hard turn means "never mind" — cancel silently. A timeout is a
            // genuine failure and earns the denial cue.
            CancelPendingIntent(/*bBroadcastFailure*/ true);
            if (bTimedOut && !bPlayerOverrode)
            {
                FireDenialCue();
            }
            // Fall through: Layer 1 still runs this frame.
        }
        else
        {
            const float SlewStep = FMath::Min(BufferedSlewDegPerSec * DeltaTime, AlignmentErrorDegrees);
            ApplyYaw(CurrentYaw + FMath::Sign(SignedError) * SlewStep);

#if !UE_BUILD_SHIPPING
            if (bDrawDebug && GEngine)
            {
                GEngine->AddOnScreenDebugMessage(0x6B64A1, 0.f, FColor::Orange,
                    FString::Printf(TEXT("[Register] BUFFERED  err=%.2f  t=%.2fs"),
                        AlignmentErrorDegrees, IntentTimeRemaining));
            }
#endif
            return;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // LAYER 1 — the detent
    // ═══════════════════════════════════════════════════════════════════════
    const bool bInArc = (AlignmentErrorDegrees <= GetCaptureArcDegrees());
    const bool bSettling =
        bYawAssistEnabled &&
        bInArc &&
        AlignmentErrorDegrees <= SettleDeadZoneDegrees &&
        PlayerYawRateDegPerSec < AssistFadeEndRate;

    if (bSettling)
    {
        // Hard settle. Sub-degree residue would otherwise chatter the Locked
        // state boundary and machine-gun the detent tick.
        ApplyYaw(SnappedYaw);
    }
    else
    {
        const float Step = ComputeAssistStep(DeltaTime, SignedError);
        if (Step > KINDA_SMALL_NUMBER)
        {
            ApplyYaw(CurrentYaw + FMath::Sign(SignedError) * Step);
        }
        else
        {
            // No write this frame — but still record where we left the yaw so
            // next frame's player-rate measurement stays correct.
            LastWrittenYaw = CurrentYaw;
            bHasWrittenYaw = true;
        }
    }

#if !UE_BUILD_SHIPPING
    if (bDrawDebug && GEngine)
    {
        static const TCHAR* StateNames[] = { TEXT("Idle"), TEXT("Seeking"), TEXT("LOCKED") };
        GEngine->AddOnScreenDebugMessage(0x6B64A1, 0.f,
            (RegisterState == EkdRegisterState::Locked) ? FColor::Green : FColor::Cyan,
            FString::Printf(
                TEXT("[Register] %s  err=%5.2f  reg=%.2f  tol=%.1f  arc=%.1f  playerRate=%6.1f"),
                StateNames[static_cast<uint8>(RegisterState)],
                AlignmentErrorDegrees, Register01,
                ResolvedToleranceDegrees, GetCaptureArcDegrees(),
                PlayerYawRateDegPerSec));
    }
#endif
}

// =============================================================================
// Evaluation
// =============================================================================

void UkdCrushAlignmentComponent::EvaluateRegister(float CurrentYaw)
{
    // Passing 45 as the tolerance makes the library return the NEAREST cardinal
    // unconditionally while still filling in the correct shortest-arc error.
    // Reusing it (instead of re-deriving) keeps the yaw -> EkdCrushDirection
    // mapping in exactly one place — the same place the ability reads it from.
    float Error = 0.f;
    const EkdCrushDirection Nearest =
        UkdCrushDirectionLibrary::ResolveCrushDirection(CurrentYaw, 45.f, Error);

    AlignmentErrorDegrees = Error;

    const float Tol = ResolvedToleranceDegrees;
    const float Arc = GetCaptureArcDegrees();

    PredictedDirection = (Error <= Arc) ? Nearest : EkdCrushDirection::None;

    // Two-band normalisation: flat 1 inside the tolerance (so the UI reads
    // "locked", not "almost"), linear ramp across the assist band outside it.
    Register01 = (Error <= Tol)
        ? 1.f
        : 1.f - FMath::Clamp((Error - Tol) / FMath::Max(Arc - Tol, KINDA_SMALL_NUMBER), 0.f, 1.f);

    SetRegisterState(
        (Error <= Tol) ? EkdRegisterState::Locked :
        (Error <= Arc) ? EkdRegisterState::Seeking :
        EkdRegisterState::Idle);
}

float UkdCrushAlignmentComponent::ComputeAssistStep(float DeltaTime, float SignedError) const
{
    if (!bYawAssistEnabled || MaxAssistDegPerSec <= 0.f || DeltaTime <= 0.f)
    {
        return 0.f;
    }

    const float Error = FMath::Abs(SignedError);
    const float Arc = GetCaptureArcDegrees();

    if (Error > Arc || Error <= SettleDeadZoneDegrees)
    {
        return 0.f;
    }

    // Proximity shaping: near zero at the arc edge so crossing INTO the arc is
    // imperceptible (nothing "grabs" the camera), rising smoothly toward centre.
    const float Proximity01 = 1.f - (Error / Arc);
    const float Shape = FMath::SmoothStep(0.f, 1.f, Proximity01);

    // Speed gate: full assist while still or making fine adjustments, zero while
    // sweeping. This is the difference between "magnetic" and "input lag".
    const float FadeEnd = FMath::Max(AssistFadeEndRate, AssistFadeStartRate + 1.f);
    const float SpeedGate = 1.f - FMath::SmoothStep(AssistFadeStartRate, FadeEnd, PlayerYawRateDegPerSec);

    const float RatePerSec = MaxAssistDegPerSec * Shape * SpeedGate;

    // Clamping the step to the remaining error makes the settle critically
    // damped by construction — it can never overshoot, so no spring to tune and
    // no oscillation to debug.
    return FMath::Min(RatePerSec * DeltaTime, Error);
}

float UkdCrushAlignmentComponent::GetCaptureArcDegrees() const
{
    const float Raw = ResolvedToleranceDegrees * FMath::Max(1.2f, CaptureArcMultiplier);

    // 44 deg hard cap. At 45 the arcs of adjacent cardinals would meet, and the
    // predicted direction could flip under the player mid-buffer — the exact
    // "the game did something I didn't ask for" failure this system exists to
    // prevent. The cap is structural, not a tuning value.
    return FMath::Clamp(Raw, ResolvedToleranceDegrees + 1.f, 44.f);
}

bool UkdCrushAlignmentComponent::IsRegisterAvailable() const
{
    if (!CachedPlayer || !CachedPC || !CachedASC)
    {
        return false;
    }

    const FkdGameplayTags& Tags = FkdGameplayTags::Get();

    // Already 2D: yaw belongs to the crush camera, and exit needs no alignment.
    if (CachedASC->HasMatchingGameplayTag(Tags.State_CrushMode))     return false;

    // Mid-morph: UkdCrushTransitionComponent owns the camera outright.
    if (CachedASC->HasMatchingGameplayTag(Tags.State_Transitioning)) return false;

    // A corpse does not aim.
    if (CachedASC->HasMatchingGameplayTag(Tags.State_Dead))          return false;

    // Exhausted: the ability is blocked anyway. Showing a "ready" register here
    // would be an outright lie — the press would still fail.
    if (CachedASC->HasMatchingGameplayTag(Tags.State_Exhausted))     return false;

    // Tutorial locks: no crush yet, or no camera control yet.
    if (CachedASC->HasMatchingGameplayTag(Tags.Tutorial_Lock_Crush)) return false;
    if (CachedASC->HasMatchingGameplayTag(Tags.Tutorial_Lock_Look))  return false;

    return true;
}

// =============================================================================
// Yaw write — the ONE place this component touches control rotation
// =============================================================================

void UkdCrushAlignmentComponent::ApplyYaw(float NewYaw)
{
    if (!CachedPC)
    {
        return;
    }

    FRotator Rotation = CachedPC->GetControlRotation();
    Rotation.Yaw = NewYaw;                      // pitch and roll are never touched
    CachedPC->SetControlRotation(Rotation);

    LastWrittenYaw = FRotator::ClampAxis(NewYaw);
    bHasWrittenYaw = true;
}

// =============================================================================
// State transitions
// =============================================================================

void UkdCrushAlignmentComponent::SetRegisterState(EkdRegisterState NewState)
{
    if (NewState == RegisterState)
    {
        return;
    }

    const EkdRegisterState OldState = RegisterState;
    RegisterState = NewState;

    if (NewState == EkdRegisterState::Locked)
    {
        PlayTick(RegisterLockSound);
    }
    else if (OldState == EkdRegisterState::Locked)
    {
        PlayTick(RegisterUnlockSound);
    }

    OnRegisterStateChanged.Broadcast(RegisterState, PredictedDirection);
}

void UkdCrushAlignmentComponent::ResetRegister()
{
    Register01 = 0.f;
    AlignmentErrorDegrees = 90.f;
    PredictedDirection = EkdCrushDirection::None;

    // Routed through SetRegisterState so the unlock tick and the broadcast still
    // fire when crush mode / death yanks the register away mid-lock.
    SetRegisterState(EkdRegisterState::Idle);
}

// =============================================================================
// Intent buffer
// =============================================================================

EkdCrushIntentResult UkdCrushAlignmentComponent::RequestCrushIntent()
{
    // Re-entrant call from our own FireBufferedIntent. We are aligned by
    // definition at that point; answering Immediate stops a second latch.
    if (bFiringBufferedIntent)
    {
        return EkdCrushIntentResult::Immediate;
    }

    if (!IsRegisterAvailable())
    {
        // Not our call to make (exhausted, transitioning, dead, locked out).
        // The caller's existing path already handles every one of these.
        return EkdCrushIntentResult::NotApplicable;
    }

    // The press can land between ticks — re-evaluate against live yaw so the
    // decision is never made on a stale frame.
    EvaluateRegister(FRotator::ClampAxis(CachedPC->GetControlRotation().Yaw));

    if (RegisterState == EkdRegisterState::Locked)
    {
        return EkdCrushIntentResult::Immediate;
    }

    if (bIntentBufferEnabled && RegisterState == EkdRegisterState::Seeking)
    {
        if (!bIntentPending)
        {
            // A repeat press inside a live window is absorbed, NOT re-armed.
            // Re-arming would let a mashing player extend the slew indefinitely.
            bIntentPending = true;
            IntentTimeRemaining = IntentBufferTime;

#if !UE_BUILD_SHIPPING
            UE_LOG(LogTemp, Log,
                TEXT("[Register] Intent BUFFERED | err=%.2f deg | window=%.2fs | dir=%d"),
                AlignmentErrorDegrees, IntentBufferTime, static_cast<int32>(PredictedDirection));
#endif
        }
        return EkdCrushIntentResult::Buffered;
    }

    // Genuinely off-axis (or buffering disabled). This is the only remaining
    // case that reaches the player as a refusal — and by now the register has
    // been visibly at zero, so it reads as "not aimed", not "button broken".
#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log,
        TEXT("[Register] Intent DENIED | err=%.2f deg > arc=%.2f deg"),
        AlignmentErrorDegrees, GetCaptureArcDegrees());
#endif

    FireDenialCue();
    OnCrushIntentResolved.Broadcast(false);
    return EkdCrushIntentResult::Denied;
}

void UkdCrushAlignmentComponent::CancelPendingIntent(bool bBroadcastFailure)
{
    if (!bIntentPending)
    {
        return;
    }

    bIntentPending = false;
    IntentTimeRemaining = 0.f;

    if (bBroadcastFailure)
    {
        OnCrushIntentResolved.Broadcast(false);
    }
}

void UkdCrushAlignmentComponent::FireBufferedIntent()
{
    bIntentPending = false;
    IntentTimeRemaining = 0.f;

    if (!CachedPlayer)
    {
        return;
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("[Register] Intent FIRED | err=%.3f deg | dir=%d"),
        AlignmentErrorDegrees, static_cast<int32>(PredictedDirection));
#endif

    // Manual guard rather than TGuardValue: the call below re-enters
    // RequestCrushIntent through AkdMyPlayer, and this flag is what makes that
    // re-entry answer Immediate instead of latching a second buffer.
    bFiringBufferedIntent = true;
    CachedPlayer->RequestCrushToggle();
    bFiringBufferedIntent = false;

    OnCrushIntentResolved.Broadcast(true);
}

// =============================================================================
// Cues
// =============================================================================

void UkdCrushAlignmentComponent::PlayTick(USoundBase* Sound) const
{
    if (!Sound)
    {
        return;   // silent by design when the designer has not assigned a cue
    }

    const UWorld* World = GetWorld();
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    if (!GI)
    {
        return;
    }

    // All SFX go through the subsystem — never UGameplayStatics directly.
    if (UkdAudioSubsystem* Audio = GI->GetSubsystem<UkdAudioSubsystem>())
    {
        Audio->PlaySFX2D(Sound, RegisterTickVolume);
    }
}

void UkdCrushAlignmentComponent::FireDenialCue() const
{
    if (!CachedPlayer)
    {
        return;
    }

    // NotifyCrushDenied already routes to UkdAudioSubsystem::PlayCrushDenied,
    // so this is the single correct entry point — do not double-fire audio here.
    if (UkdGameFeedbackComponent* Feedback = CachedPlayer->GetGameFeedbackComponent())
    {
        Feedback->NotifyCrushDenied();
    }
}

// =============================================================================
// Tolerance resolution
// =============================================================================

float UkdCrushAlignmentComponent::ResolveToleranceFromAbilityCDO()
{
    if (CachedASC)
    {
        // Walking the granted specs (rather than DefaultAbilities) works with the
        // Blueprint subclasses that are actually granted, and does not depend on
        // FindAbilitySpecFromClass, which requires an exact class match.
        for (const FGameplayAbilitySpec& Spec : CachedASC->GetActivatableAbilities())
        {
            if (const Ukd_CrushToggle* Toggle = Cast<Ukd_CrushToggle>(Spec.Ability))
            {
                bToleranceResolved = true;
                return FMath::Clamp(Toggle->GetAlignmentToleranceDegrees(), 1.f, 44.f);
            }
        }
    }

    // Not an error on the first frames — abilities are granted after component
    // BeginPlay. TickComponent keeps retrying until the spec appears.
    return FallbackToleranceDegrees;
}
