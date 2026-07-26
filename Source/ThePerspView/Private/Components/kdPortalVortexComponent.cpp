// Copyright ASKD Games

#include "Components/kdPortalVortexComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/kdAttributeSet.h"
#include "UI/ColorLibrary/kdThemeAccess.h"
#include "UI/ColorLibrary/kdHUDColorLibrary.h"

// ── Material parameter names — must match M_ShadowPortal exactly ────────────────
const FName UkdPortalVortexComponent::MP_Charge = TEXT("Charge");
const FName UkdPortalVortexComponent::MP_Opacity = TEXT("Opacity");
const FName UkdPortalVortexComponent::MP_GlowColor = TEXT("GlowColor");

namespace
{
	// Charge is considered "arrived" within this epsilon (constant lerps snap clean).
	constexpr float kChargeEpsilon = 0.005f;
	constexpr float kOpacityEpsilon = 0.004f;
	constexpr float kGlowEpsilonSq = 0.0001f; // squared colour distance
}

UkdPortalVortexComponent::UkdPortalVortexComponent()
{
	// Can tick, but starts DISABLED. The material's Time node runs the perpetual
	// swirl/breathing for free; we only enable ticks during transitions.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UkdPortalVortexComponent::InitializeForMesh(UStaticMeshComponent* InMesh, UAbilitySystemComponent* InASC)
{
	PortalMesh = InMesh;
	CachedASC = InASC;

	if (!PortalMesh)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("PortalVortex [%s]: InitializeForMesh called with null mesh."),
			*GetNameSafe(GetOwner()));
#endif
		return;
	}

	// Build the one dynamic material this component owns end-to-end.
	if (UMaterialInterface* Base = PortalMesh->GetMaterial(0))
	{
		DynMat = UMaterialInstanceDynamic::Create(Base, this);
		PortalMesh->SetMaterial(0, DynMat);
	}
#if !UE_BUILD_SHIPPING
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PortalVortex [%s]: mesh has no material at slot 0."),
			*GetNameSafe(GetOwner()));
	}
#endif

	// Neon-preserve stencil so UkdWorldColorDriver doesn't desaturate the glow.
	if (NeonStencilValue > 0)
	{
		PortalMesh->SetRenderCustomDepth(true);
		PortalMesh->SetCustomDepthStencilValue(NeonStencilValue);
	}

	// Event-driven glow: re-lerp whenever stamina changes, even from tick-off idle.
	if (CachedASC.IsValid())
	{
		CachedASC->GetGameplayAttributeValueChangeDelegate(
			UkdAttributeSet::GetShadowStaminaAttribute())
			.AddUObject(this, &UkdPortalVortexComponent::OnStaminaChanged);
	}

	// Seed the glow immediately so the first frame isn't white.
	CurrentGlow = ResolveTargetGlow();

	// Portal is created hidden by the actor; stay dormant until OnPortalShown().
	SetPhase(EkdPortalVortexPhase::Dormant);
	PushParams();
}

// ── Orchestration hooks ────────────────────────────────────────────────────────

void UkdPortalVortexComponent::OnPortalShown()
{
	// Bloom open from a collapsed pinpoint.
	Charge = 1.f;
	bOnCooldown = false;
	TargetOpacity = 1.f;
	SetPhase(EkdPortalVortexPhase::Blooming);
}

void UkdPortalVortexComponent::OnPortalHidden()
{
	// Reset so the next show blooms cleanly again.
	Charge = 1.f;
	ChargeTarget = 1.f;
	SetPhase(EkdPortalVortexPhase::Dormant); // disables tick via RefreshTickState
}

void UkdPortalVortexComponent::OnCollapse()
{
	// Player swallowed HERE: rapid inhale. On completion the machine auto-blooms
	// back open (dim, because the actor will also flag cooldown this frame).
	SetPhase(EkdPortalVortexPhase::Collapsing);
}

void UkdPortalVortexComponent::OnExitBloom()
{
	// Player arrived HERE: exhale outward from a pinpoint.
	Charge = 1.f;
	SetPhase(EkdPortalVortexPhase::Blooming);
}

void UkdPortalVortexComponent::OnCooldownComplete()
{
	SetCooldownState(false); // brighten back to armed
}

void UkdPortalVortexComponent::SetCooldownState(bool bInOnCooldown)
{
	bOnCooldown = bInOnCooldown;
	TargetOpacity = bOnCooldown ? CooldownOpacity : 1.f;
	RefreshTickState();
}

// ── State machine ──────────────────────────────────────────────────────────────

void UkdPortalVortexComponent::SetPhase(EkdPortalVortexPhase NewPhase)
{
	Phase = NewPhase;

	switch (Phase)
	{
	case EkdPortalVortexPhase::Collapsing:
		ChargeTarget = 1.f;
		ChargeRate = 1.f / FMath::Max(CollapseSeconds, KINDA_SMALL_NUMBER);
		break;

	case EkdPortalVortexPhase::Blooming:
		ChargeTarget = 0.f;
		ChargeRate = 1.f / FMath::Max(BloomSeconds, KINDA_SMALL_NUMBER);
		break;

	case EkdPortalVortexPhase::Idle:
		ChargeTarget = 0.f;
		ChargeRate = 0.f;
		break;

	case EkdPortalVortexPhase::Dormant:
	default:
		ChargeRate = 0.f;
		break;
	}

	RefreshTickState();
}

void UkdPortalVortexComponent::RefreshTickState()
{
	SetComponentTickEnabled(NeedsTick());
}

bool UkdPortalVortexComponent::NeedsTick() const
{
	if (Phase == EkdPortalVortexPhase::Dormant)
	{
		return false;
	}

	const bool bChargeMoving = FMath::Abs(Charge - ChargeTarget) > kChargeEpsilon;
	const bool bOpacityMoving = FMath::Abs(CurrentOpacity - TargetOpacity) > kOpacityEpsilon;

	const FLinearColor Diff = ResolveTargetGlow() - CurrentGlow;
	const bool bGlowMoving =
		(Diff.R * Diff.R + Diff.G * Diff.G + Diff.B * Diff.B) > kGlowEpsilonSq;

	return bChargeMoving || bOpacityMoving || bGlowMoving;
}

// ── Tick ───────────────────────────────────────────────────────────────────────

void UkdPortalVortexComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Shape: constant-rate lerp for predictable, punchy timing.
	Charge = FMath::FInterpConstantTo(Charge, ChargeTarget, DeltaTime, ChargeRate);

	// Availability + glow: eased lerps for a soft settle.
	CurrentOpacity = FMath::FInterpTo(CurrentOpacity, TargetOpacity, DeltaTime, OpacityInterpSpeed);
	CurrentGlow = FMath::CInterpTo(CurrentGlow, ResolveTargetGlow(), DeltaTime, GlowInterpSpeed);

	// Auto-advance the shape machine.
	if (Phase == EkdPortalVortexPhase::Collapsing &&
		FMath::Abs(Charge - 1.f) <= kChargeEpsilon)
	{
		SetPhase(EkdPortalVortexPhase::Blooming); // reopen after the inhale
	}
	else if (Phase == EkdPortalVortexPhase::Blooming &&
		FMath::Abs(Charge) <= kChargeEpsilon)
	{
		SetPhase(EkdPortalVortexPhase::Idle);
	}

	PushParams();

	// Disable ourselves the moment everything has settled.
	if (!NeedsTick())
	{
		SetComponentTickEnabled(false);
	}
}

// ── Data flow ──────────────────────────────────────────────────────────────────

float UkdPortalVortexComponent::ReadStaminaFraction() const
{
	if (!CachedASC.IsValid())
	{
		return 1.f;
	}

	const float Max = CachedASC->GetNumericAttribute(UkdAttributeSet::GetMaxShadowStaminaAttribute());
	const float Cur = CachedASC->GetNumericAttribute(UkdAttributeSet::GetShadowStaminaAttribute());

	return (Max > 0.f) ? FMath::Clamp(Cur / Max, 0.f, 1.f) : 1.f;
}

FLinearColor UkdPortalVortexComponent::ResolveTargetGlow() const
{
	// PaleIon when usable → Steelgrey when stamina-starved.
	if (UkdColorTheme* Theme = UkdThemeAccess::GetColorTheme(this))
	{
		return UkdHUDColorLibrary::GetPortalGlowColor(Theme, ReadStaminaFraction());
	}
	return CurrentGlow; // driver not ready yet — hold last value, no flash.
}

void UkdPortalVortexComponent::PushParams() const
{
	if (!DynMat)
	{
		return;
	}
	DynMat->SetScalarParameterValue(MP_Charge, Charge);
	DynMat->SetScalarParameterValue(MP_Opacity, CurrentOpacity);
	DynMat->SetVectorParameterValue(MP_GlowColor, CurrentGlow);
}

void UkdPortalVortexComponent::OnStaminaChanged(const FOnAttributeChangeData& /*Data*/)
{
	// Only meaningful while the portal is on-screen (not Dormant). Kick the tick
	// so the glow re-lerps; it will disable itself once settled.
	if (Phase != EkdPortalVortexPhase::Dormant)
	{
		RefreshTickState();
	}
}
