// Copyright ASKD Games

#include "Crush/kdCrushTransitionComponent.h"
#include "Player/kdMyPlayer.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/TimelineComponent.h"

// ─────────────────────────────────────────────────────────────────────────────

UkdCrushTransitionComponent::UkdCrushTransitionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    CrushTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("CrushTimeline"));
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay — capture all 3D baselines.  Never touch these again until the
// component is re-initialized (level reload / respawn).
// ─────────────────────────────────────────────────────────────────────────────

void UkdCrushTransitionComponent::BeginPlay()
{
    Super::BeginPlay();

    CachedOwner = CastChecked<AkdMyPlayer>(GetOwner());

    if (USkeletalMeshComponent* Mesh = CachedOwner->GetMesh())
        Original3DScale = Mesh->GetRelativeScale3D();

    if (CachedOwner->Camera)
        Original3DFOV = CachedOwner->Camera->FieldOfView;

    if (CachedOwner->SpringArm)
    {
        Original3DArmLength = CachedOwner->SpringArm->TargetArmLength;
        Original3DArmRotation = CachedOwner->SpringArm->GetRelativeRotation();
    }

    // Wire timeline (curve is optional — falls back to linear if null)
    if (!TransitionCurve)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("CrushTransition [%s]: No TransitionCurve assigned — using linear."),
            *GetOwner()->GetName());
    }

    FOnTimelineFloat UpdateDelegate;
    UpdateDelegate.BindUFunction(this, FName("HandleTimelineUpdate"));
    CrushTimeline->AddInterpFloat(TransitionCurve, UpdateDelegate);

    FOnTimelineEvent FinishedDelegate;
    FinishedDelegate.BindUFunction(this, FName("HandleTimelineFinished"));
    CrushTimeline->SetTimelineFinishedFunc(FinishedDelegate);
}

// ─────────────────────────────────────────────────────────────────────────────
// StartTransition
//
// Everything from frame 0: player snap, freeze, scale punch, and the start of
// the camera and mesh lerp ranges are all set here before the timeline plays.
// ─────────────────────────────────────────────────────────────────────────────

void UkdCrushTransitionComponent::StartTransition(bool bToCrushMode)
{
#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("CrushTransition [%s]: → %s"),
        *GetOwner()->GetName(), bToCrushMode ? TEXT("2D") : TEXT("3D"));
#endif

    if (!CachedOwner) return;

    // Cancel any running transition cleanly.
    GetWorld()->GetTimerManager().ClearTimer(SettleTickHandle);
    CrushTimeline->Stop();

    bTargetCrushMode = bToCrushMode;
    bYawLocked = false;
    bMovementRestored = false;

    // Compute what fraction of the full timeline is "anticipation".
    // The camera starts moving at t=0; the mesh waits until AnticipationRatio.
    const float TotalDuration = AnticipationDuration + TransitionDuration;
    AnticipationRatio = (TotalDuration > KINDA_SMALL_NUMBER)
        ? AnticipationDuration / TotalDuration
        : 0.f;

    // ── Set timeline play rate from total duration ────────────────────────────
    CrushTimeline->SetPlayRate(1.f / FMath::Max(TotalDuration, KINDA_SMALL_NUMBER));

    // ── *** THE CRUSH: snap player X to the shadow plane *** ─────────────────
    // This is the physical act of pressing the player flat.  Combined with the
    // camera pulling back and narrowing, the world appears to collapse.
    if (bToCrushMode)
    {
        FVector Loc = CachedOwner->GetActorLocation();
        Loc.X = CrushWorldX;
        CachedOwner->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
    }

    // ── Player freeze + scale punch ───────────────────────────────────────────
    if (UCharacterMovementComponent* MC = CachedOwner->GetCharacterMovement())
    {
        MC->StopMovementImmediately();
        MC->DisableMovement();
    }

    if (USkeletalMeshComponent* Mesh = CachedOwner->GetMesh())
    {
        const FVector CurrentScale = Mesh->GetRelativeScale3D();
        const float   PunchFraction = bToCrushMode
            ? AnticipationScalePunch
            : -AnticipationScalePunch * 0.5f;
        Mesh->SetRelativeScale3D(CurrentScale * (1.f + PunchFraction));

        // Mesh lerp: FROM = punched scale, TO = target
        MeshScaleFrom = Mesh->GetRelativeScale3D();
        MeshScaleTo = bToCrushMode ? PlayerCrushScale : Original3DScale;
    }

    // ── Camera lerp ranges ────────────────────────────────────────────────────
    if (CachedOwner->Camera)
    {
        FOVFrom = CachedOwner->Camera->FieldOfView;
        FOVTo = bToCrushMode ? Crush2DFOV : Original3DFOV;
    }

    if (CachedOwner->SpringArm)
    {
        ArmLengthFrom = CachedOwner->SpringArm->TargetArmLength;
        ArmLengthTo = bToCrushMode ? Crush2DArmLength : Original3DArmLength;

        ArmRotFrom = CachedOwner->SpringArm->GetRelativeRotation();
        ArmRotTo = bToCrushMode ? Crush2DArmRotation : Original3DArmRotation;
    }

    CrushTimeline->PlayFromStart();
}

// ─────────────────────────────────────────────────────────────────────────────
// HandleTimelineUpdate
//
// Single callback drives camera AND mesh.
// Value is already curve-evaluated — never call GetFloatValue(Value) again.
//
// Phases by alpha:
//   [0 … AnticipationRatio]  : camera moves; player frozen; mesh unchanged
//   [AnticipationRatio … 1]  : movement restored; mesh lerps; camera continues
//   [~0.5]                   : yaw lock / unlock fires once (mid-transition)
//
// ─────────────────────────────────────────────────────────────────────────────

void UkdCrushTransitionComponent::HandleTimelineUpdate(float Value)
{
    if (!CachedOwner) return;

    const float Alpha = Value;   // curve-shaped [0, 1]

    // ── Restore movement after anticipation phase ─────────────────────────────
    if (!bMovementRestored && Alpha >= AnticipationRatio)
    {
        bMovementRestored = true;
        if (UCharacterMovementComponent* MC = CachedOwner->GetCharacterMovement())
            MC->SetMovementMode(MOVE_Walking);
    }

    // ── Player mesh scale (active only after anticipation) ────────────────────
    if (Alpha >= AnticipationRatio)
    {
        const float MeshAlpha = FMath::Clamp(
            (Alpha - AnticipationRatio) / FMath::Max(1.f - AnticipationRatio, KINDA_SMALL_NUMBER),
            0.f, 1.f);

        if (USkeletalMeshComponent* Mesh = CachedOwner->GetMesh())
            Mesh->SetRelativeScale3D(FMath::Lerp(MeshScaleFrom, MeshScaleTo, MeshAlpha));
    }

    // ── Camera — FOV + arm length + arm rotation + roll (entire timeline) ─────
    // Camera starts moving at t=0 so the world visually begins shifting the
    // instant the button is pressed, even while the player is frozen.
    const float CamAlpha = Alpha;

    const float NewFOV = FMath::Lerp(FOVFrom, FOVTo, CamAlpha);
    const float NewArmLength = FMath::Lerp(ArmLengthFrom, ArmLengthTo, CamAlpha);
    const FRotator NewArmRot = FMath::Lerp(ArmRotFrom, ArmRotTo, CamAlpha);

    // Camera roll: sine arch that peaks at mid-transition and returns to zero.
    // Entering crush → negative roll (world tilts one way).
    // Exiting  crush → positive roll (world tilts back).
    // The effect is subtle (default 3°) but gives the transition physical weight.
    const float RollPeak = bTargetCrushMode ? -TransitionRollDegrees : TransitionRollDegrees;
    const float CurrentRoll = RollPeak * FMath::Sin(Alpha * PI);   // arch: 0 → peak → 0

    ApplyCameraState(NewFOV, NewArmLength, NewArmRot, CurrentRoll);

    // ── Mid-point: lock / unlock yaw (fires exactly once per transition) ──────
    if (!bYawLocked && Alpha >= 0.5f)
    {
        bYawLocked = true;

        if (CachedOwner->SpringArm)
        {
            CachedOwner->SpringArm->bInheritYaw = !bTargetCrushMode;

#if !UE_BUILD_SHIPPING
            UE_LOG(LogTemp, Log, TEXT("CrushTransition: YawInherit=%s at alpha=%.2f"),
                bTargetCrushMode ? TEXT("false") : TEXT("true"), Alpha);
#endif
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// HandleTimelineFinished
// ─────────────────────────────────────────────────────────────────────────────

void UkdCrushTransitionComponent::HandleTimelineFinished()
{
    if (!CachedOwner) return;

    // Guarantee movement is restored if anticipation was skipped.
    if (!bMovementRestored)
    {
        bMovementRestored = true;
        if (UCharacterMovementComponent* MC = CachedOwner->GetCharacterMovement())
            MC->SetMovementMode(MOVE_Walking);
    }

    // Hard-snap everything to exact targets — eliminates floating-point residual.
    if (CachedOwner->Camera)
        CachedOwner->Camera->SetFieldOfView(FOVTo);

    if (CachedOwner->SpringArm)
    {
        CachedOwner->SpringArm->TargetArmLength = ArmLengthTo;
        CachedOwner->SpringArm->SetRelativeRotation(ArmRotTo);

        // Clear roll that was applied during transition.
        FRotator CleanRot = CachedOwner->SpringArm->GetRelativeRotation();
        CleanRot.Roll = 0.f;
        CachedOwner->SpringArm->SetRelativeRotation(CleanRot);
    }

    // Gameplay state updates fire BEFORE the visual settle.
    OnTransitionComplete.Broadcast(bTargetCrushMode);

    // ── Settle overshoot ──────────────────────────────────────────────────────
    if (SettleOvershootAmount > KINDA_SMALL_NUMBER && SettleDuration > KINDA_SMALL_NUMBER)
    {
        if (USkeletalMeshComponent* Mesh = CachedOwner->GetMesh())
            Mesh->SetRelativeScale3D(MeshScaleTo * (1.f + SettleOvershootAmount));

        SettleElapsed = 0.f;
        GetWorld()->GetTimerManager().SetTimer(
            SettleTickHandle, this,
            &UkdCrushTransitionComponent::SettleTick,
            1.f / 60.f, true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SettleTick — exponential decay from overshoot back to exact target.
// ─────────────────────────────────────────────────────────────────────────────

void UkdCrushTransitionComponent::SettleTick()
{
    if (!CachedOwner) return;

    constexpr float kRate = 1.f / 60.f;
    SettleElapsed += kRate;

    const float t = FMath::Clamp(SettleElapsed / FMath::Max(SettleDuration, kRate), 0.f, 1.f);
    const float Ease = 1.f - FMath::Exp(-6.f * t);   // fast snap, smooth tail

    USkeletalMeshComponent* Mesh = CachedOwner->GetMesh();
    if (!Mesh)
    {
        GetWorld()->GetTimerManager().ClearTimer(SettleTickHandle);
        return;
    }

    if (t >= 1.f)
    {
        Mesh->SetRelativeScale3D(MeshScaleTo);
        GetWorld()->GetTimerManager().ClearTimer(SettleTickHandle);
        return;
    }

    Mesh->SetRelativeScale3D(
        FMath::Lerp(MeshScaleTo * (1.f + SettleOvershootAmount), MeshScaleTo, Ease));
}

// ─────────────────────────────────────────────────────────────────────────────
// ApplyCameraState — single call site for all camera writes.
// ─────────────────────────────────────────────────────────────────────────────

void UkdCrushTransitionComponent::ApplyCameraState(
    float FOV, float ArmLen, const FRotator& ArmRot, float Roll)
{
    if (!CachedOwner) return;

    if (CachedOwner->Camera)
        CachedOwner->Camera->SetFieldOfView(FOV);

    if (CachedOwner->SpringArm)
    {
        CachedOwner->SpringArm->TargetArmLength = ArmLen;

        // Apply rotation with the roll component added separately.
        FRotator Applied = ArmRot;
        Applied.Roll = Roll;
        CachedOwner->SpringArm->SetRelativeRotation(Applied);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// RestoreCameraDefaults — called on respawn / level reload.
// ─────────────────────────────────────────────────────────────────────────────

void UkdCrushTransitionComponent::RestoreCameraDefaults()
{
    GetWorld()->GetTimerManager().ClearTimer(SettleTickHandle);
    CrushTimeline->Stop();

    ApplyCameraState(Original3DFOV, Original3DArmLength, Original3DArmRotation, 0.f);

    if (CachedOwner && CachedOwner->SpringArm)
    {
        CachedOwner->SpringArm->bInheritYaw = true;

        FRotator Clean = CachedOwner->SpringArm->GetRelativeRotation();
        Clean.Roll = 0.f;
        CachedOwner->SpringArm->SetRelativeRotation(Clean);
    }

    if (CachedOwner)
    {
        if (USkeletalMeshComponent* Mesh = CachedOwner->GetMesh())
            Mesh->SetRelativeScale3D(Original3DScale);
    }
}
