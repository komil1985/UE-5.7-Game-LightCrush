// Copyright ASKD Games

#include "Components/kdStrategicCameraComponent.h"
#include "Camera/CameraComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/SpringArmComponent.h"


DEFINE_LOG_CATEGORY_STATIC(LogkdStrategicCamera, Log, All);

UkdStrategicCameraComponent::UkdStrategicCameraComponent()
{
	// Tick is opt-in: enabled only while a transition is in flight, then self-disabled.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Camera framing should update after movement so the rig reads the final pawn transform.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UkdStrategicCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	// Auto-wire if Initialize() was not called explicitly. Harmless if already wired.
	if (!IsValid(SpringArm) || !IsValid(Camera))
	{
		ResolveRig();
	}
}

void UkdStrategicCameraComponent::Initialize(USpringArmComponent* InSpringArm, UCameraComponent* InCamera)
{
	SpringArm = InSpringArm;
	Camera = InCamera;
}

bool UkdStrategicCameraComponent::ResolveRig()
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return false;
	}

	if (!IsValid(SpringArm))
	{
		SpringArm = Owner->FindComponentByClass<USpringArmComponent>();
	}
	if (!IsValid(Camera))
	{
		Camera = Owner->FindComponentByClass<UCameraComponent>();
	}

	if (!IsValid(SpringArm) || !IsValid(Camera))
	{
		UE_LOG(LogkdStrategicCamera, Warning,
			TEXT("[%s] Could not resolve a spring arm + camera on the owner. Strategic view is disabled. Call Initialize() explicitly if your rig is non-standard."),
			*GetNameSafe(Owner));
		return false;
	}

	return true;
}

void UkdStrategicCameraComponent::RequestStrategicView(bool bEngage)
{
	// Lazily resolve the rig so the component is robust even if BeginPlay ordering changes.
	if (!IsValid(SpringArm) || !IsValid(Camera))
	{
		if (!ResolveRig())
		{
			return;
		}
	}

	// Capture the live gameplay baseline only when starting a fresh engagement from
	// full rest. Mid-flight re-engages keep the original baseline so a quick
	// tap -> release -> tap never drifts.
	if (bEngage && !bBaseCached && CurrentAlpha <= 0.0f)
	{
		CacheBaseState();
	}

	bEngaged = bEngage;

	// Any change of intent means we need to interpolate; make sure tick is live.
	SetComponentTickEnabled(true);
}

void UkdStrategicCameraComponent::ForceReturnImmediate()
{
	bEngaged = false;
	CurrentAlpha = 0.0f;

	OnSurveyAlphaChanged.Broadcast(0.f);            // hard reset clears any survey blur instantly

	if (bBaseCached)
	{
		RestoreBaseState();
	}

	SetComponentTickEnabled(false);
}

void UkdStrategicCameraComponent::CacheBaseState()
{
	BaseArmLength = SpringArm->TargetArmLength;
	BaseFOV = Camera->FieldOfView;
	BasePitch = SpringArm->GetRelativeRotation().Pitch;
	bBaseCollisionTest = SpringArm->bDoCollisionTest;

	// Survey pitch must never fight the rotation source. If the arm is driven by
	// pawn control rotation, a relative-pitch write is overwritten every frame, so
	// we cleanly skip pitch and let arm + FOV carry the strategic read instead.
	bCanApplySurveyPitch = bApplySurveyPitch && !SpringArm->bUsePawnControlRotation;

	if (bDisableCollisionDuringView)
	{
		// Let the dolly-back pull cleanly past geometry behind the player rather
		// than snapping short on a wall. Restored verbatim on full return.
		SpringArm->bDoCollisionTest = false;
	}

	bBaseCached = true;
}

void UkdStrategicCameraComponent::RestoreBaseState()
{
	if (IsValid(SpringArm))
	{
		SpringArm->TargetArmLength = BaseArmLength;
		SpringArm->bDoCollisionTest = bBaseCollisionTest;

		if (bCanApplySurveyPitch)
		{
			FRotator R = SpringArm->GetRelativeRotation();
			R.Pitch = BasePitch;
			SpringArm->SetRelativeRotation(R);
		}
	}

	if (IsValid(Camera))
	{
		Camera->SetFieldOfView(BaseFOV);
	}

	bBaseCached = false;
	bCanApplySurveyPitch = false;
}

void UkdStrategicCameraComponent::ApplyAlpha(float Eased)
{
	// Dolly-back + FOV widen applied together to cancel dolly-zoom warp.
	SpringArm->TargetArmLength = FMath::Lerp(BaseArmLength, BaseArmLength * StrategicArmMultiplier, Eased);
	Camera->SetFieldOfView(FMath::Lerp(BaseFOV, BaseFOV + StrategicFOVDelta, Eased));

	if (bCanApplySurveyPitch)
	{
		// Bounded pitch with no wrap, so a scalar lerp is correct and cheaper than
		// FQuat::Slerp here (Slerp is reserved for deltas that may cross +/-180).
		FRotator R = SpringArm->GetRelativeRotation();
		R.Pitch = FMath::Lerp(BasePitch, BasePitch + SurveyPitchDelta, Eased);
		SpringArm->SetRelativeRotation(R);
	}
}

void UkdStrategicCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(SpringArm) || !IsValid(Camera) || !bBaseCached)
	{
		// Nothing valid to drive — stand down rather than spin.
		SetComponentTickEnabled(false);
		return;
	}

	// Advance the raw alpha toward its target at the appropriate (asymmetric) rate.
	const float Target = bEngaged ? 1.0f : 0.0f;
	if (Target > CurrentAlpha)
	{
		CurrentAlpha = FMath::Min(Target, CurrentAlpha + DeltaTime / FMath::Max(EngageTime, KINDA_SMALL_NUMBER));
	}
	else
	{
		CurrentAlpha = FMath::Max(Target, CurrentAlpha - DeltaTime / FMath::Max(ReturnTime, KINDA_SMALL_NUMBER));
	}

	// Shape it: designer curve if provided, otherwise a smoothstep ease.
	const float Eased = IsValid(EaseCurve) ? EaseCurve->GetFloatValue(CurrentAlpha)
		: FMath::SmoothStep(0.0f, 1.0f, CurrentAlpha);

	ApplyAlpha(Eased);

	OnSurveyAlphaChanged.Broadcast(CurrentAlpha);   // raw 0..1; the driver shapes its own ramp

	// Settle: once we reach the resting target, stop ticking. On a full return we
	// also restore the exact baseline and release the cache so the next engage
	// re-captures fresh live values.
	const bool bSettled = (bEngaged && CurrentAlpha >= 1.0f) || (!bEngaged && CurrentAlpha <= 0.0f);
	if (bSettled)
	{
		if (!bEngaged)
		{
			RestoreBaseState();
		}
		SetComponentTickEnabled(false);
	}
}
