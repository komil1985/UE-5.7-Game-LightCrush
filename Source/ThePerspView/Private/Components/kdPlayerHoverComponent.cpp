// Copyright ASKD Games


#include "Components/kdPlayerHoverComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"



UkdPlayerHoverComponent::UkdPlayerHoverComponent()
{
	// Run AFTER the character movement tick so velocity is already settled.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UkdPlayerHoverComponent::BeginPlay()
{
	Super::BeginPlay();

    // Snapshot the mesh's designer-placed relative offset so the hover sine
    // is always measured from that origin and never drifts.
    if (BodyMesh)
    {
        MeshBaseOffset = BodyMesh->GetRelativeLocation();
    }

    // Snapshot eye base scale — restored at the end of every blink.
    if (!EyeMeshes.IsEmpty() && EyeMeshes[0])
    {
        EyeBaseScale = EyeMeshes[0]->GetRelativeScale3D();
    }

    // Stagger the very first blink so it doesn't fire at t=0.
    ScheduleNextBlink();
}

void UkdPlayerHoverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    TickHover(DeltaTime);
    TickBlink(DeltaTime);
}

void UkdPlayerHoverComponent::SetMeshComponents(USkeletalMeshComponent* InBodyMesh, TArray<UStaticMeshComponent*> InEyeMeshes)
{
    BodyMesh = InBodyMesh;

    EyeMeshes.Reset(InEyeMeshes.Num());
    for (UStaticMeshComponent* Eye : InEyeMeshes)
    {
        if (Eye)
        {
            EyeMeshes.Add(Eye);
        }
    }
}

void UkdPlayerHoverComponent::TickHover(float DeltaTime)
{
    if (!BodyMesh) return;

    // ── Determine target amplitude from lateral speed ─────────────────────────
    // We use the owner's velocity 2D (horizontal plane) so vertical jumps or
    // falls during Crush Mode don't incorrectly trigger the "moving" state.
    const ACharacter* Owner = Cast<ACharacter>(GetOwner());
    const float HorizontalSpeed = Owner ? Owner->GetVelocity().Size2D() : 0.f;

    const float TargetAmplitude = (HorizontalSpeed > IdleSpeedThreshold) ? MovingAmplitude : IdleAmplitude;

    // ── Smooth blend so amplitude doesn't snap on direction change ────────────
    CurrentAmplitude = FMath::FInterpTo(CurrentAmplitude, TargetAmplitude, DeltaTime, AmplitudeBlendSpeed);

    // ── Advance phase (radians) — frame-rate independent ─────────────────────
    // Wrapping keeps the float from losing precision over long play sessions.
    HoverPhase += DeltaTime * HoverFrequency * UE_TWO_PI;
    if (HoverPhase > UE_TWO_PI)
    {
        HoverPhase -= UE_TWO_PI;
    }

    // ── Apply offset to mesh relative location ────────────────────────────────
    // Only Z is touched — XY stay at the designer-set baseline so the
    // capsule collision and world-space actor position remain correct.
    FVector NewRelLoc = MeshBaseOffset;
    NewRelLoc.Z += CurrentAmplitude * FMath::Sin(HoverPhase);
    BodyMesh->SetRelativeLocation(NewRelLoc);
}

void UkdPlayerHoverComponent::TickBlink(float DeltaTime)
{
    // Nothing to do if idle or no eyes registered.
    if (BlinkPhase == EBlinkPhase::Idle || EyeMeshes.IsEmpty()) return;

    BlinkTimer += DeltaTime;

    switch (BlinkPhase)
    {
        // ── Closing: ease-in for a snappy, natural blink onset ────────────────
    case EBlinkPhase::Closing:
    {
        const float Alpha = FMath::Clamp(BlinkTimer / BlinkCloseTime, 0.f, 1.f);
        const float EasedAlpha = Alpha * Alpha;         // quadratic ease-in

        ApplyEyeScale(FMath::Lerp(EyeBaseScale[BlinkAxis], EyeBaseScale[BlinkAxis] * BlinkClosedScale, EasedAlpha));

        if (BlinkTimer >= BlinkCloseTime)
        {
            // Snap exactly closed to eliminate float drift.
            ApplyEyeScale(EyeBaseScale[BlinkAxis] * BlinkClosedScale);
            BlinkPhase = EBlinkPhase::Holding;
            BlinkTimer = 0.f;
        }
        break;
    }

    // ── Holding: eye stays fully closed ───────────────────────────────────
    case EBlinkPhase::Holding:
    {
        if (BlinkTimer >= BlinkHoldTime)
        {
            BlinkPhase = EBlinkPhase::Opening;
            BlinkTimer = 0.f;
        }
        break;
    }

    // ── Opening: ease-out for a softer re-open ────────────────────────────
    case EBlinkPhase::Opening:
    {
        const float Alpha = FMath::Clamp(BlinkTimer / BlinkOpenTime, 0.f, 1.f);
        const float EasedAlpha = 1.f - (1.f - Alpha) * (1.f - Alpha); // quadratic ease-out

        ApplyEyeScale(FMath::Lerp(EyeBaseScale[BlinkAxis] * BlinkClosedScale, EyeBaseScale[BlinkAxis], EasedAlpha));

        if (BlinkTimer >= BlinkOpenTime)
        {
            // Snap back to exact original scale — no accumulated float error.
            ApplyEyeScale(EyeBaseScale[BlinkAxis]);
            BlinkPhase = EBlinkPhase::Idle;
            BlinkTimer = 0.f;
            ScheduleNextBlink();
        }
        break;
    }

    default: break;
    }
}

void UkdPlayerHoverComponent::ScheduleNextBlink()
{
    UWorld* World = GetWorld();
    if (!World) return;

    const float Delay = FMath::FRandRange(BlinkIntervalMin, BlinkIntervalMax);

    World->GetTimerManager().SetTimer(BlinkScheduleHandle, this, &UkdPlayerHoverComponent::OnBlinkTimerFired, Delay, /*bLoop=*/false);
}

void UkdPlayerHoverComponent::StartBlink()
{
    // Guard: never interrupt an in-progress blink.
    if (BlinkPhase != EBlinkPhase::Idle) return;

    BlinkPhase = EBlinkPhase::Closing;
    BlinkTimer = 0.f;
}

void UkdPlayerHoverComponent::ApplyEyeScale(float AxisValue)
{
    // Build a scale vector that matches the base on all axes, then override
    // only the designated blink axis.
    FVector NewScale = EyeBaseScale;

    // BlinkAxis: 0=X, 1=Y, 2=Z — validated at edit-time via ClampMin/Max meta.
    NewScale[FMath::Clamp(BlinkAxis, 0, 2)] = AxisValue;

    for (TObjectPtr<UStaticMeshComponent>& Eye : EyeMeshes)
    {
        if (Eye)
        {
            Eye->SetRelativeScale3D(NewScale);
        }
    }
}

void UkdPlayerHoverComponent::OnBlinkTimerFired()
{
    StartBlink();
}

