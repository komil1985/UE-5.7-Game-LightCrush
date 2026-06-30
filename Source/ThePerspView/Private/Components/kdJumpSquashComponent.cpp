// Copyright ASKD Games

#include "Components/kdJumpSquashComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayTags/kdGameplayTags.h"

namespace
{
	FORCEINLINE float Smoothstep(float Alpha)
	{
		Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
		return Alpha * Alpha * (3.f - 2.f * Alpha);
	}
}

UkdJumpSquashComponent::UkdJumpSquashComponent()
{
	// Same tick group as UkdPlayerHoverComponent: run AFTER CharacterMovement
	// resolves so Velocity.Z reflects this frame's actual physics state.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UkdJumpSquashComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedOwner = Cast<ACharacter>(GetOwner());
	if (!CachedOwner)
	{
		UE_LOG(LogTemp, Error, TEXT("UkdJumpSquashComponent: Owner is not an ACharacter — disabled."));
		return;
	}

	BodyMesh = CachedOwner->GetMesh();
	CachedMoveComp = CachedOwner->GetCharacterMovement();

	if (BodyMesh)
	{
		BodyBaseScale = BodyMesh->GetRelativeScale3D();
	}

	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(CachedOwner))
	{
		CachedASC = ASI->GetAbilitySystemComponent();
	}

	bWasFalling = CachedMoveComp && CachedMoveComp->IsFalling();

	// Same multicast delegate UkdFallDamageComponent binds to for fall
	// damage thresholds — multiple listeners are fine, this is not a
	// single-writer field.
	CachedOwner->LandedDelegate.AddDynamic(this, &UkdJumpSquashComponent::OnCharacterLanded);
}

bool UkdJumpSquashComponent::IsSuspendedForCrushMode() const
{
	return CachedASC && CachedASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode);
}

void UkdJumpSquashComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!BodyMesh || !CachedMoveComp) return;

	// ── Crush Mode hand-off ────────────────────────────────────────────────
	// UkdCrushTransitionComponent owns BodyMesh scale entirely while in
	// Crush Mode. Writing here at the same time would be a double-drive
	// bug, so every frame in Crush Mode we zero our own offsets (without
	// touching the mesh — CrushTransitionComponent is already driving it)
	// and bail until the player exits again.
	if (IsSuspendedForCrushMode())
	{
		AirborneScaleOffset = LaunchPopOffset = ApexPuffOffset = LandingOffset = FVector::ZeroVector;
		bLaunchPopActive = bApexPuffActive = bLandingActive = false;
		return;
	}

	// ── Detect liftoff: grounded -> Falling with upward velocity ────────────
	const bool bIsFalling = CachedMoveComp->IsFalling();
	if (bIsFalling && !bWasFalling)
	{
		const float VZ = CachedMoveComp->Velocity.Z;
		if (VZ >= MinLaunchSpeed)
		{
			bLaunchPopActive = true;
			LaunchPopElapsed = 0.f;
			OnJumpLaunched.Broadcast(VZ);
		}
	}
	bWasFalling = bIsFalling;

	// ── Tick active one-shot layers ───────────────────────────────────────
	if (bLaunchPopActive) TickLaunchPop(DeltaTime);
	if (bApexPuffActive)  TickApexPuff(DeltaTime);
	if (bLandingActive)   TickLanding(DeltaTime);

	// ── Continuous velocity-driven layer ────────────────────────────────────
	TickAirborne(DeltaTime);

	ApplyFinalScale();
}

void UkdJumpSquashComponent::TickAirborne(float DeltaTime)
{
	FVector Target = FVector::ZeroVector;

	if (CachedMoveComp->IsFalling())
	{
		const float VZ = CachedMoveComp->Velocity.Z;
		if (VZ > 0.f)
		{
			// Rising: stretch tall, thin in XY.
			const float Alpha = FMath::Clamp(VZ / FMath::Max(RiseSpeedForFullStretch, 1.f), 0.f, 1.f);
			const float ZStretch = Alpha * MaxRiseStretch;
			Target = FVector(-ZStretch * 0.5f, -ZStretch * 0.5f, ZStretch);
		}
		else
		{
			// Falling: squash short, wide in XY — visually anticipates impact.
			const float Alpha = FMath::Clamp(-VZ / FMath::Max(FallSpeedForFullSquash, 1.f), 0.f, 1.f);
			const float ZSquash = Alpha * MaxFallSquash;
			Target = FVector(ZSquash * 0.5f, ZSquash * 0.5f, -ZSquash);
		}
	}

	// Chase the velocity-derived target rather than snapping, so the pose
	// reads as continuous through the rise -> apex -> fall transition, and
	// decays smoothly back to neutral once grounded.
	AirborneScaleOffset = FMath::VInterpTo(AirborneScaleOffset, Target, DeltaTime, AirborneBlendSpeed);
}

void UkdJumpSquashComponent::TickLaunchPop(float DeltaTime)
{
	LaunchPopElapsed += DeltaTime;
	const float Duration = FMath::Max(LaunchPopDuration, KINDA_SMALL_NUMBER);
	const float t = FMath::Clamp(LaunchPopElapsed / Duration, 0.f, 1.f);

	const float CompressEnd = LaunchCompressFraction;
	const float StretchPeak = FMath::Max(LaunchStretchPeakFraction, CompressEnd + 0.05f);

	// Three-phase curve: 0 -> -Compress -> +Stretch -> 0. Each phase uses
	// the same smoothstep-lerp the rest of the project already relies on
	// (see UkdCrushTransitionComponent's dip/pop exit shape).
	float ZOffset;
	if (t <= CompressEnd)
	{
		const float a = Smoothstep(t / CompressEnd);
		ZOffset = FMath::Lerp(0.f, -LaunchCompressAmount, a);
	}
	else if (t <= StretchPeak)
	{
		const float a = Smoothstep((t - CompressEnd) / (StretchPeak - CompressEnd));
		ZOffset = FMath::Lerp(-LaunchCompressAmount, LaunchStretchAmount, a);
	}
	else
	{
		const float a = Smoothstep((t - StretchPeak) / (1.f - StretchPeak));
		ZOffset = FMath::Lerp(LaunchStretchAmount, 0.f, a);
	}

	// Volume-preserving inverse on XY, half strength.
	const float XYOffset = -ZOffset * 0.5f;
	LaunchPopOffset = FVector(XYOffset, XYOffset, ZOffset);

	if (t >= 1.f)
	{
		bLaunchPopActive = false;
		LaunchPopOffset = FVector::ZeroVector;
	}
}

void UkdJumpSquashComponent::TickApexPuff(float DeltaTime)
{
	ApexPuffElapsed += DeltaTime;
	const float t = FMath::Clamp(ApexPuffElapsed / FMath::Max(ApexPuffDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);

	// Single smooth hump (0 -> peak -> 0), uniform on all axes — a small
	// weightless "puff" rather than a directional squash/stretch.
	const float Hump = FMath::Sin(t * PI);
	const float Puff = Hump * ApexPuffAmount;
	ApexPuffOffset = FVector(Puff, Puff, Puff);

	if (t >= 1.f)
	{
		bApexPuffActive = false;
		ApexPuffOffset = FVector::ZeroVector;
	}
}

void UkdJumpSquashComponent::TickLanding(float DeltaTime)
{
	LandingElapsed += DeltaTime;
	const float Duration = FMath::Max(LandingDurationActual, KINDA_SMALL_NUMBER);
	const float t = FMath::Clamp(LandingElapsed / Duration, 0.f, 1.f);

	const float SquashEnd = 0.30f; // fast impact snap
	const float ReboundEnd = 0.62f; // overshoot into a small stretch

	// Overshoot scales with how hard the squash was, so a tiny step-down
	// doesn't get the same rebound pop as a hard fall.
	const float ScaledOvershoot = LandingOvershootAmount *
		(LandingPeakSquash / FMath::Max(LandingSquashAmount, KINDA_SMALL_NUMBER));

	float ZOffset;
	if (t <= SquashEnd)
	{
		// Cubic ease-out — snaps to full squash almost immediately, reads
		// as a real impact rather than a slow tween.
		const float a = 1.f - FMath::Pow(1.f - (t / SquashEnd), 3.f);
		ZOffset = FMath::Lerp(0.f, -LandingPeakSquash, a);
	}
	else if (t <= ReboundEnd)
	{
		const float a = Smoothstep((t - SquashEnd) / (ReboundEnd - SquashEnd));
		ZOffset = FMath::Lerp(-LandingPeakSquash, ScaledOvershoot, a);
	}
	else
	{
		const float a = Smoothstep((t - ReboundEnd) / (1.f - ReboundEnd));
		ZOffset = FMath::Lerp(ScaledOvershoot, 0.f, a);
	}

	const float XYOffset = -ZOffset * 0.5f;
	LandingOffset = FVector(XYOffset, XYOffset, ZOffset);

	if (t >= 1.f)
	{
		bLandingActive = false;
		LandingOffset = FVector::ZeroVector;
	}
}

void UkdJumpSquashComponent::ApplyFinalScale()
{
	if (!BodyMesh) return;

	const FVector TotalOffset = AirborneScaleOffset + LaunchPopOffset + ApexPuffOffset + LandingOffset;

	// Multiplicative against the designer-authored base scale (1 + offset
	// per axis) so this layers cleanly on top of whatever base scale
	// BP_Player was set up with, rather than assuming (1,1,1).
	const FVector FinalScale(
		BodyBaseScale.X * (1.f + TotalOffset.X),
		BodyBaseScale.Y * (1.f + TotalOffset.Y),
		BodyBaseScale.Z * (1.f + TotalOffset.Z));

	BodyMesh->SetRelativeScale3D(FinalScale);
}

void UkdJumpSquashComponent::NotifyApex()
{
	if (IsSuspendedForCrushMode()) return;

	if (ApexPuffAmount > 0.f)
	{
		bApexPuffActive = true;
		ApexPuffElapsed = 0.f;
	}
	OnJumpApex.Broadcast();
}

void UkdJumpSquashComponent::OnCharacterLanded(const FHitResult& Hit)
{
	if (IsSuspendedForCrushMode()) return;
	if (!CachedMoveComp) return;

	// Velocity at the instant Landed fires still holds the pre-landing
	// value — same technique UkdFallDamageComponent uses for its read.
	const float ImpactSpeed = FMath::Abs(CachedMoveComp->Velocity.Z);
	if (ImpactSpeed < MinLandingSpeed) return;

	const float Alpha = FMath::Clamp(
		(ImpactSpeed - MinLandingSpeed) / FMath::Max(MaxLandingSpeedForFullSquash - MinLandingSpeed, 1.f),
		0.f, 1.f);

	LandingPeakSquash = LandingSquashAmount * Alpha;

	// Harder impacts settle slightly slower — reads as "heavier" without a
	// second curve asset.
	LandingDurationActual = LandingSettleDuration * FMath::Lerp(0.85f, 1.25f, Alpha);

	bLandingActive = true;
	LandingElapsed = 0.f;

	// Any in-flight launch pop or apex puff is superseded by the impact —
	// cancel them so stale offsets don't fight the landing squash.
	bLaunchPopActive = false;
	LaunchPopOffset = FVector::ZeroVector;
	bApexPuffActive = false;
	ApexPuffOffset = FVector::ZeroVector;

	OnJumpLanded.Broadcast(ImpactSpeed, ImpactSpeed >= HardLandThreshold);
}