// Copyright ASKD Games

#include "Crush/kdCrushTransitionComponent.h"
#include "Player/kdMyPlayer.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/TimelineComponent.h"
#include "Crush/kdCrushDirectionLibrary.h"

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
    {
        Original3DScale = Mesh->GetRelativeScale3D();
        OriginalMeshRelativeLoc = Mesh->GetRelativeLocation();
    }

    if (CachedOwner->Camera)
    {
        Original3DFOV = CachedOwner->Camera->FieldOfView;
    }

    if (CachedOwner->SpringArm)
    {
        //Original3DArmLength = CachedOwner->SpringArm->TargetArmLength;
        //Original3DArmRotation = CachedOwner->SpringArm->GetRelativeRotation();

        Original3DArmLength = CachedOwner->SpringArm->TargetArmLength;
        Original3DArmRotQ = CachedOwner->SpringArm->GetRelativeRotation().Quaternion();
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

void UkdCrushTransitionComponent::StartTransition(bool bToCrushMode, EkdCrushDirection Direction)
{
#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("CrushTransition [%s]: → %s"),
        *GetOwner()->GetName(), bToCrushMode ? TEXT("2D") : TEXT("3D"));
#endif

    if (!CachedOwner) return;

    if (bToCrushMode)
    {
        ActiveCrushDirection = Direction;
    }

    GetWorld()->GetTimerManager().ClearTimer(SettleTickHandle);
    CrushTimeline->Stop();

    bTargetCrushMode = bToCrushMode;
    bYawLocked = false;
    bMovementRestored = false;

    const float TotalDuration = AnticipationDuration + TransitionDuration;
    AnticipationRatio = (TotalDuration > KINDA_SMALL_NUMBER)
        ? AnticipationDuration / TotalDuration
        : 0.f;

    CrushTimeline->SetPlayRate(1.f / FMath::Max(TotalDuration, KINDA_SMALL_NUMBER));

    // Apply X plane constraint before anything moves — player cannot fall
    // even if the floor geometry hasn't slid to X = CrushWorldX yet.
    //if (bToCrushMode) ApplyPlaneConstraint();

    // Snap player X to shadow plane.
    //if (bToCrushMode)
    //{
    //    FVector Loc = CachedOwner->GetActorLocation();
    //    Loc.X = CrushWorldX;
    //    CachedOwner->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
    //}

    // Freeze movement.
    if (UCharacterMovementComponent* MC = CachedOwner->GetCharacterMovement())
    {
        MC->StopMovementImmediately();
        MC->DisableMovement();
    }

    // Scale punch (uniform, anticipation only).
    if (USkeletalMeshComponent* Mesh = CachedOwner->GetMesh())
    {
        const float PunchFraction = bToCrushMode
            ? 0.f   // no uniform punch entering — the Z stretch IS the anticipation
            : -0.f;  // no uniform punch exiting  — the Z dip handles it
        // We skip the old uniform punch here; the Z squash-stretch replaces it
        // with a more directional, physical feel.

        MeshScaleFrom = Mesh->GetRelativeScale3D();
        MeshScaleTo = bToCrushMode ? PlayerCrushScale : Original3DScale;
    }

    // Camera ranges.
    if (CachedOwner->Camera)
    {
        FOVFrom = CachedOwner->Camera->FieldOfView;
        FOVTo = bToCrushMode ? Crush2DFOV : Original3DFOV;
    }

    //if (CachedOwner->SpringArm)
    //{
    //    ArmLengthFrom = CachedOwner->SpringArm->TargetArmLength;
    //    ArmLengthTo = bToCrushMode ? Crush2DArmLength : Original3DArmLength;
    //    ArmRotFromQ = CachedOwner->SpringArm->GetRelativeRotation().Quaternion();
    //    ArmRotToQ = (bToCrushMode
    //        ? Crush2DArmRotation
    //        : Original3DArmRotQ.Rotator()).Quaternion();
    //}

    if (CachedOwner->SpringArm)
    {
        ArmLengthFrom = CachedOwner->SpringArm->TargetArmLength;
        ArmLengthTo = bToCrushMode ? Crush2DArmLength : Original3DArmLength;
        ArmRotFromQ = CachedOwner->SpringArm->GetRelativeRotation().Quaternion();

        if (bToCrushMode)
        {
            // Aim the crush camera along the active collapse axis so it always
            // frames a true 2D side-view — and so WalkRight lines up with the
            // camera's right vector, keeping controls consistent on every axis.
            // Keep the tuned telephoto pitch; only the yaw is direction-driven.
            const FkdCrushBasis Basis = UkdCrushDirectionLibrary::MakeCrushBasis(
                CachedOwner->GetActiveCrushDirection());

            const FRotator CrushArmRot(Crush2DArmRotation.Pitch, Basis.CameraYaw, 0.f);
            ArmRotToQ = CrushArmRot.Quaternion();
        }
        else
        {
            ArmRotToQ = Original3DArmRotQ; // already a quat
        }
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
    //    if (!CachedOwner) return;
    //
    //    const float Alpha = Value;   // curve-shaped [0, 1]
    //
    //    // ── Restore movement after anticipation phase ─────────────────────────────
    //    if (!bMovementRestored && Alpha >= AnticipationRatio)
    //    {
    //        bMovementRestored = true;
    //        if (UCharacterMovementComponent* MC = CachedOwner->GetCharacterMovement())
    //            MC->SetMovementMode(MOVE_Walking);
    //    }
    //
    //    //// ── Player mesh scale (active only after anticipation) ────────────────────
    //    //if (Alpha >= AnticipationRatio)
    //    //{
    //    //    const float MeshAlpha = FMath::Clamp(
    //    //        (Alpha - AnticipationRatio) / FMath::Max(1.f - AnticipationRatio, KINDA_SMALL_NUMBER),
    //    //        0.f, 1.f);
    //
    //    //    if (USkeletalMeshComponent* Mesh = CachedOwner->GetMesh())
    //    //        Mesh->SetRelativeScale3D(FMath::Lerp(MeshScaleFrom, MeshScaleTo, MeshAlpha));
    //    //}
    //
    //    // ── Uniform mesh scale (starts after anticipation) ────────────────────────
    //    USkeletalMeshComponent* Mesh = CachedOwner->GetMesh();
    //    if (Mesh && Alpha >= AnticipationRatio)
    //    {
    //        const float MeshAlpha = FMath::Clamp((Alpha - AnticipationRatio) / FMath::Max(1.f - AnticipationRatio, KINDA_SMALL_NUMBER), 0.f, 1.f);
    //
    //        Mesh->SetRelativeScale3D(FMath::Lerp(MeshScaleFrom, MeshScaleTo, MeshAlpha));
    //    }
    //
    //    // ── Z squash-stretch (runs the entire timeline, modifies scale set above) ──
    //    // Must run AFTER the uniform scale write — it reads the current scale as
    //    // its base and applies per-axis multipliers on top.
    //    ApplyZSquashStretch(Alpha, Mesh);
    //
    //    // ── Camera — FOV + arm length + arm rotation + roll (entire timeline) ─────
    //    // Camera starts moving at t=0 so the world visually begins shifting the
    //    // instant the button is pressed, even while the player is frozen.
    //    const float CamAlpha = Alpha;
    //
    //    const float NewFOV = FMath::Lerp(FOVFrom, FOVTo, CamAlpha);
    //    const float NewArmLength = FMath::Lerp(ArmLengthFrom, ArmLengthTo, CamAlpha);
    //    const FRotator NewArmRot = FMath::Lerp(ArmRotFrom, ArmRotTo, CamAlpha);
    //
    //    // Camera roll: sine arch that peaks at mid-transition and returns to zero.
    //    // Entering crush → negative roll (world tilts one way).
    //    // Exiting  crush → positive roll (world tilts back).
    //    // The effect is subtle (default 3°) but gives the transition physical weight.
    //    const float RollPeak = bTargetCrushMode ? -TransitionRollDegrees : TransitionRollDegrees;
    //    const float CurrentRoll = RollPeak * FMath::Sin(Alpha * PI);   // arch: 0 → peak → 0
    //
    //    FVector Loc = CachedOwner->GetActorLocation();
    //    Loc.X = FMath::Lerp(PlayerXFrom, PlayerXTo, Alpha);
    //    CachedOwner->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
    //
    //    ApplyCameraState(NewFOV, NewArmLength, NewArmRot, CurrentRoll);
    //
    //    // ── Mid-point: lock / unlock yaw (fires exactly once per transition) ──────
    //    if (!bYawLocked && Alpha >= 0.5f)
    //    {
    //        bYawLocked = true;
    //
    //        if (CachedOwner->SpringArm)
    //        {
    //            CachedOwner->SpringArm->bInheritYaw = !bTargetCrushMode;
    //
    //#if !UE_BUILD_SHIPPING
    //            UE_LOG(LogTemp, Log, TEXT("CrushTransition: YawInherit=%s at alpha=%.2f"),
    //                bTargetCrushMode ? TEXT("false") : TEXT("true"), Alpha);
    //#endif
    //        }
    //    }

    if (!CachedOwner) return;

    const float Alpha = Value;   // curve-evaluated, never call GetFloatValue again

    // ── Restore movement after anticipation ───────────────────────────────────
    if (!bMovementRestored && Alpha >= AnticipationRatio)
    {
        bMovementRestored = true;
        if (UCharacterMovementComponent* MC = CachedOwner->GetCharacterMovement())
            MC->SetMovementMode(MOVE_Walking);
    }

    // ── Uniform mesh scale (starts after anticipation) ────────────────────────
    USkeletalMeshComponent* Mesh = CachedOwner->GetMesh();
    if (Mesh && Alpha >= AnticipationRatio)
    {
        const float MeshAlpha = FMath::Clamp(
            (Alpha - AnticipationRatio)
            / FMath::Max(1.f - AnticipationRatio, KINDA_SMALL_NUMBER),
            0.f, 1.f);

        Mesh->SetRelativeScale3D(FMath::Lerp(MeshScaleFrom, MeshScaleTo, MeshAlpha));
    }

    // ── Z squash-stretch (runs the entire timeline, modifies scale set above) ──
    // Must run AFTER the uniform scale write — it reads the current scale as
    // its base and applies per-axis multipliers on top.
    ApplyZSquashStretch(Alpha, Mesh);

    // ── Camera: FOV + arm length + arm rotation (Slerp, entire timeline) ──────
    if (CachedOwner->Camera)
        CachedOwner->Camera->SetFieldOfView(FMath::Lerp(FOVFrom, FOVTo, Alpha));

    if (CachedOwner->SpringArm)
    {
        CachedOwner->SpringArm->TargetArmLength = FMath::Lerp(ArmLengthFrom, ArmLengthTo, Alpha);

        // Slerp: always takes the shortest arc regardless of ±180° boundary.
        const FQuat SlerpedQ = FQuat::Slerp(ArmRotFromQ, ArmRotToQ, Alpha);
        FRotator Applied = SlerpedQ.Rotator();
        Applied.Roll = (bTargetCrushMode ? -TransitionRollDegrees : TransitionRollDegrees)
            * FMath::Sin(Alpha * PI);
        CachedOwner->SpringArm->SetRelativeRotation(Applied);
    }

    // ── Yaw lock at midpoint (fires exactly once) ─────────────────────────────
    if (!bYawLocked && Alpha >= 0.5f)
    {
        bYawLocked = true;
        if (CachedOwner->SpringArm)
            CachedOwner->SpringArm->bInheritYaw = !bTargetCrushMode;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// HandleTimelineFinished
// ─────────────────────────────────────────────────────────────────────────────

void UkdCrushTransitionComponent::HandleTimelineFinished()
{
    //if (!CachedOwner) return;

    //// Guarantee movement is restored if anticipation was skipped.
    //if (!bMovementRestored)
    //{
    //    bMovementRestored = true;
    //    if (UCharacterMovementComponent* MC = CachedOwner->GetCharacterMovement())
    //        MC->SetMovementMode(MOVE_Walking);
    //}

    //// Hard-snap everything to exact targets — eliminates floating-point residual.
    //if (CachedOwner->Camera) CachedOwner->Camera->SetFieldOfView(FOVTo);

    //FVector Loc = CachedOwner->GetActorLocation();
    //Loc.X = PlayerXTo;
    //CachedOwner->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);

    //if (CachedOwner->SpringArm)
    //{
    //    CachedOwner->SpringArm->TargetArmLength = ArmLengthTo;
    //    CachedOwner->SpringArm->SetRelativeRotation(ArmRotTo);

    //    // Clear roll that was applied during transition.
    //    FRotator CleanRot = CachedOwner->SpringArm->GetRelativeRotation();
    //    CleanRot.Roll = 0.f;
    //    CachedOwner->SpringArm->SetRelativeRotation(CleanRot);
    //}

    //// Gameplay state updates fire BEFORE the visual settle.
    //OnTransitionComplete.Broadcast(bTargetCrushMode);

    //// ── Settle overshoot ──────────────────────────────────────────────────────
    //if (SettleOvershootAmount > KINDA_SMALL_NUMBER && SettleDuration > KINDA_SMALL_NUMBER)
    //{
    //    if (USkeletalMeshComponent* Mesh = CachedOwner->GetMesh())
    //        Mesh->SetRelativeScale3D(MeshScaleTo * (1.f + SettleOvershootAmount));

    //    SettleElapsed = 0.f;
    //    GetWorld()->GetTimerManager().SetTimer(
    //        SettleTickHandle, this,
    //        &UkdCrushTransitionComponent::SettleTick,
    //        1.f / 60.f, true);
    //}

    if (!CachedOwner) return;

    if (!bMovementRestored)
    {
        bMovementRestored = true;
        if (UCharacterMovementComponent* MC = CachedOwner->GetCharacterMovement())
            MC->SetMovementMode(MOVE_Walking);
    }

    // ── Hard-snap everything to exact targets ─────────────────────────────────

    if (CachedOwner->Camera)
        CachedOwner->Camera->SetFieldOfView(FOVTo);

    if (CachedOwner->SpringArm)
    {
        CachedOwner->SpringArm->TargetArmLength = ArmLengthTo;
        FRotator Final = ArmRotToQ.Rotator();
        Final.Roll = 0.f;
        CachedOwner->SpringArm->SetRelativeRotation(Final);
    }

    // Restore mesh to exact base scale + original location.
    if (USkeletalMeshComponent* Mesh = CachedOwner->GetMesh())
    {
        Mesh->SetRelativeScale3D(MeshScaleTo);
        Mesh->SetRelativeLocation(OriginalMeshRelativeLoc);
    }

    // Gameplay state (tags, floor trace, shadow tracking) fires before settle.
    OnTransitionComplete.Broadcast(bTargetCrushMode);

    // ── Settle overshoot (uniform scale only — location is already clean) ─────
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
    //if (!CachedOwner) return;

    //constexpr float kRate = 1.f / 60.f;
    //SettleElapsed += kRate;

    //const float t = FMath::Clamp(SettleElapsed / FMath::Max(SettleDuration, kRate), 0.f, 1.f);
    //const float Ease = 1.f - FMath::Exp(-6.f * t);   // fast snap, smooth tail

    //USkeletalMeshComponent* Mesh = CachedOwner->GetMesh();
    //if (!Mesh)
    //{
    //    GetWorld()->GetTimerManager().ClearTimer(SettleTickHandle);
    //    return;
    //}

    //if (t >= 1.f)
    //{
    //    Mesh->SetRelativeScale3D(MeshScaleTo);
    //    GetWorld()->GetTimerManager().ClearTimer(SettleTickHandle);
    //    return;
    //}

    //Mesh->SetRelativeScale3D(
    //    FMath::Lerp(MeshScaleTo * (1.f + SettleOvershootAmount), MeshScaleTo, Ease));

    if (!CachedOwner) return;

    constexpr float kRate = 1.f / 60.f;
    SettleElapsed += kRate;

    const float t = FMath::Clamp(SettleElapsed / FMath::Max(SettleDuration, kRate), 0.f, 1.f);
    const float Ease = 1.f - FMath::Exp(-6.f * t);

    USkeletalMeshComponent* Mesh = CachedOwner->GetMesh();
    if (!Mesh) { GetWorld()->GetTimerManager().ClearTimer(SettleTickHandle); return; }

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

void UkdCrushTransitionComponent::ApplyCameraState(float FOV, float ArmLen, const FRotator& ArmRot, float Roll)
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

void UkdCrushTransitionComponent::ApplyZSquashStretch(float Alpha, USkeletalMeshComponent* Mesh)
{
    if (!Mesh) return;

    float ZOffset = 0.f;
    float ZScaleMult = 1.f;   // multiplier relative to the current base Z scale
    float YScaleMult = 1.f;

    if (bTargetCrushMode)
    {
        // ── ENTERING CRUSH ────────────────────────────────────────────────────
        if (Alpha < AnticipationRatio)
        {
            // Anticipation: rise and stretch
            const float t = Smooth(Alpha / FMath::Max(AnticipationRatio, KINDA_SMALL_NUMBER));
            ZOffset = EnterStretchZ * t;
            ZScaleMult = 1.f + EnterStretchScaleZ * t;
            YScaleMult = 1.f - EnterStretchScaleZ * 0.4f * t;  // subtle width change
        }
        else
        {
            // Morph: fall from peak, sink below, return
            const float morphT = (Alpha - AnticipationRatio)
                / FMath::Max(1.f - AnticipationRatio, KINDA_SMALL_NUMBER);

            // ZOffset shape:
            //   morphT=0.0 → +EnterStretchZ   (starting from stretch peak)
            //   morphT=0.7 → -EnterSinkZ       (weight dragging down)
            //   morphT=1.0 → 0                 (back to origin)
            // Implemented as two smoothstepped segments:
            const float fallT = FMath::Clamp(morphT / 0.7f, 0.f, 1.f);
            const float riseT = FMath::Clamp((morphT - 0.7f) / 0.3f, 0.f, 1.f);
            const float fallZ = FMath::Lerp(EnterStretchZ, -EnterSinkZ, Smooth(fallT));
            ZOffset = FMath::Lerp(fallZ, 0.f, Smooth(riseT));

            // Scale: return Z/Y to neutral as the uniform scale morph takes over
            ZScaleMult = FMath::Lerp(1.f + EnterStretchScaleZ, 1.f, Smooth(morphT));
            YScaleMult = FMath::Lerp(1.f - EnterStretchScaleZ * 0.4f, 1.f, Smooth(morphT));
        }
    }
    else
    {
        // ── EXITING CRUSH ─────────────────────────────────────────────────────
        if (Alpha < AnticipationRatio)
        {
            // Anticipation: dip down and squash
            const float t = Smooth(Alpha / FMath::Max(AnticipationRatio, KINDA_SMALL_NUMBER));
            ZOffset = -ExitDipZ * t;
            ZScaleMult = 1.f - ExitDipSquashZ * t;
            YScaleMult = 1.f + ExitDipSquashZ * 0.5f * t;
        }
        else
        {
            // Morph: rise from dip, overshoot to pop, return
            const float morphT = (Alpha - AnticipationRatio)
                / FMath::Max(1.f - AnticipationRatio, KINDA_SMALL_NUMBER);

            // ZOffset shape:
            //   morphT=0.0 → -ExitDipZ          (starting from dip)
            //   morphT=0.4 → +ExitPopZ           (ejected from shadow plane)
            //   morphT=1.0 → 0                   (settled)
            const float riseT = FMath::Clamp(morphT / 0.4f, 0.f, 1.f);
            const float fallT = FMath::Clamp((morphT - 0.4f) / 0.6f, 0.f, 1.f);
            const float peakZ = FMath::Lerp(-ExitDipZ, ExitPopZ, Smooth(riseT));
            ZOffset = FMath::Lerp(peakZ, 0.f, Smooth(fallT));

            // Z scale: stretch at pop, return to 1× at end
            const float stretchPeak = 1.f + ExitPopStretchZ;
            const float atRise = FMath::Lerp(1.f - ExitDipSquashZ, stretchPeak, Smooth(riseT));
            ZScaleMult = FMath::Lerp(atRise, 1.f, Smooth(fallT));
            YScaleMult = FMath::Lerp(
                1.f + ExitDipSquashZ * 0.5f,
                1.f,
                Smooth(morphT));
        }
    }

    // ── Apply location offset (Z only — X and Y untouched) ───────────────────
    FVector NewLoc = OriginalMeshRelativeLoc;
    NewLoc.Z += ZOffset;
    Mesh->SetRelativeLocation(NewLoc);

    // ── Apply per-axis scale modifiers on top of the uniform scale lerp ───────
    // The uniform lerp (MeshScaleFrom → MeshScaleTo) is computed in
    // HandleTimelineUpdate and passed in as the base.  We modify Z and Y
    // independently here — X is left to the uniform lerp.
    //
    // Read the scale that was JUST written by the uniform lerp so we modify
    // the correct base value, not a stale snapshot.
    FVector CurrentScale = Mesh->GetRelativeScale3D();
    CurrentScale.Z *= ZScaleMult;
    CurrentScale.Y *= YScaleMult;
    Mesh->SetRelativeScale3D(CurrentScale);
}

void UkdCrushTransitionComponent::ApplyPlaneConstraint()
{
    //    UCharacterMovementComponent* MC = CachedOwner ? CachedOwner->GetCharacterMovement() : nullptr;
    //    if (!MC) return;
    //
    //    // Lock to the player's CURRENT X — wherever they are standing right now.
    //    // This is the only correct plane origin; CrushWorldX is irrelevant.
    //    const float PlayerCurrentX = CachedOwner->GetActorLocation().X;
    //
    //    MC->SetPlaneConstraintEnabled(true);
    //    MC->SetPlaneConstraintNormal(FVector(1.f, 0.f, 0.f));
    //    MC->SetPlaneConstraintOrigin(FVector(PlayerCurrentX, 0.f, 0.f));
    //
    //#if !UE_BUILD_SHIPPING
    //    UE_LOG(LogTemp, Log,
    //        TEXT("CrushTransition: Plane constraint ON at player X=%.1f"), PlayerCurrentX);
    //#endif

        ///////////////////////////////////////// old //////////////////////////////////////////////////////
    //    UCharacterMovementComponent* MC = CachedOwner ? CachedOwner->GetCharacterMovement() : nullptr;
    //    if (!MC) return;
    //
    //    // Resolve the collapse axis from the stored direction.
    //    // PosX/NegX → normal (1,0,0).  PosY/NegY → normal (0,1,0).
    //    const FVector Normal = UkdCrushDirectionLibrary::DirectionToCollapseNormal(ActiveCrushDirection);
    //
    //    // Lock the player to their CURRENT position on the active axis.
    //    // Capturing the live location at toggle time (not a preset value) is what
    //    // prevents the constraint from fighting any sub-pixel position drift.
    //    const FVector PlayerLoc = CachedOwner->GetActorLocation();
    //    const FVector Origin(
    //        (Normal.X > KINDA_SMALL_NUMBER) ? PlayerLoc.X : 0.f,
    //        (Normal.Y > KINDA_SMALL_NUMBER) ? PlayerLoc.Y : 0.f,
    //        0.f);
    //
    //    MC->SetPlaneConstraintEnabled(true);
    //    MC->SetPlaneConstraintNormal(Normal);
    //    MC->SetPlaneConstraintOrigin(Origin);
    //
    //#if !UE_BUILD_SHIPPING
    //    UE_LOG(LogTemp, Log,
    //        TEXT("CrushTransition: Plane constraint ON | axis=%s | origin=(%.1f, %.1f)"),
    //        *Normal.ToString(), Origin.X, Origin.Y);
    //#endif

        //////////////////////////////////////// New ////////////////////////////////////////////////////////
    UCharacterMovementComponent* MC = CachedOwner ? CachedOwner->GetCharacterMovement() : nullptr;
    if (!MC) return;

    // Lock the collapse axis for the active direction, through the player's
    // current location — the plane passes through wherever they're standing.
    const FVector CollapseN = UkdCrushDirectionLibrary::MakeCrushBasis(
        CachedOwner->GetActiveCrushDirection()).CollapseNormal;

    MC->SetPlaneConstraintEnabled(true);
    MC->SetPlaneConstraintNormal(CollapseN);
    MC->SetPlaneConstraintOrigin(CachedOwner->GetActorLocation());

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("CrushTransition: Plane constraint ON | axis=%s"), *CollapseN.ToString());
#endif
}

void UkdCrushTransitionComponent::RemovePlaneConstraintInternal()
{
    UCharacterMovementComponent* MC = CachedOwner ? CachedOwner->GetCharacterMovement() : nullptr;
    if (MC)
    {
        MC->SetPlaneConstraintEnabled(false);
    }
}

void UkdCrushTransitionComponent::ReleasePlaneConstraint()
{
    RemovePlaneConstraintInternal();

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("CrushTransition: Plane constraint OFF"));
#endif
}