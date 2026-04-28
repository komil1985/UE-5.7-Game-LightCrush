// Copyright ASKD Games


#include "Components/kdGameFeedbackComponent.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystem/kdAbilitySystemComponent.h"
#include "AbilitySystem/kdAttributeSet.h"
#include "GameplayTags/kdGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"

UkdGameFeedbackComponent::UkdGameFeedbackComponent()
{
	// Tick starts DISABLED — we re-enable it only when post-process
	// values need to change, then disable again when they settle.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

}

void UkdGameFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	AkdMyPlayer* Player = Cast<AkdMyPlayer>(GetOwner());
	if (!Player) return;

	CachedASC = Cast<UkdAbilitySystemComponent>(Player->GetAbilitySystemComponent());
	if (!CachedASC) return;

	// Cache the post-process material instance that already lives on the player.
	// The player creates CrushPPInstance in its own BeginPlay / InitializeAbilitySystem.
	// We just grab the pointer — we never own it.
	CachedPPMaterial = Player->GetCrushPPInstance();

	const FkdGameplayTags& Tags = FkdGameplayTags::Get();

	// ── Tag event subscriptions ───────────────────────────────────────────────
	// State_InShadow fires when the player enters / exits the shadow plane.
	CachedASC->RegisterGameplayTagEvent(Tags.State_InShadow,EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UkdGameFeedbackComponent::OnInShadowTagChanged);

	// State_Exhausted fires when stamina hits zero.
	CachedASC->RegisterGameplayTagEvent(Tags.State_Exhausted,EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UkdGameFeedbackComponent::OnExhaustedTagChanged);

	// ── Stamina attribute delegate ────────────────────────────────────────────
	CachedASC->GetGameplayAttributeValueChangeDelegate(UkdAttributeSet::GetShadowStaminaAttribute()).AddUObject(this, &UkdGameFeedbackComponent::OnStaminaChanged);

	// Initialise stamina fraction from current value
	const float MaxStam = CachedASC->GetNumericAttribute(UkdAttributeSet::GetMaxShadowStaminaAttribute());
	const float CurStam = CachedASC->GetNumericAttribute(UkdAttributeSet::GetShadowStaminaAttribute());

	CurrentStaminaFrac = (MaxStam > 0.f) ? (CurStam / MaxStam) : 1.f;
}


void UkdGameFeedbackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ── Chromatic aberration decay ────────────────────────────────────────────
	if (CurrentChromatic > KINDA_SMALL_NUMBER)
	{
		CurrentChromatic = FMath::Max(0.f,CurrentChromatic - ChromaticDecaySpeed * DeltaTime);
		SetChromaticAberration(CurrentChromatic);
	}

	// ── Vignette pulse ────────────────────────────────────────────────────────
	// Only pulse when stamina is below the threshold.
	if (CurrentStaminaFrac < LowStaminaThreshold)
	{
		// How deep below the threshold? 0 = just entered, 1 = fully exhausted.
		const float Severity = 1.f - (CurrentStaminaFrac / LowStaminaThreshold);

		// Pulse amplitude scales with severity (barely pulses at 35%, full at 0%).
		const float PulseAmplitude = MaxVignetteIntensity * Severity;

		// Advance phase and compute oscillated value.
		VignettePhase += VignettePulseFrequency * 2.f * PI * DeltaTime;
		const float PulsedTarget = PulseAmplitude* (0.5f + 0.5f * FMath::Sin(VignettePhase));
		TargetVignette = PulsedTarget;
	}
	else
	{
		// Stamina is healthy — fade vignette out.
		TargetVignette = 0.f;
		VignettePhase = 0.f;   // reset phase so next entry starts from silence
	}

	// Smooth lerp toward target to avoid sharp jumps.
	CurrentVignette = FMath::FInterpTo(CurrentVignette, TargetVignette, DeltaTime, VignetteLerpSpeed);
	SetVignetteIntensity(CurrentVignette);

	// ── Auto-disable tick when nothing is animating ──────────────────────────
	if (!NeedsTickThisFrame())
	{
		SetTickEnabled(false);
	}
}

void UkdGameFeedbackComponent::OnDashPerformed()
{
	PlayCameraShake(DashShakeClass);

	// Spike chromatic aberration — decays in Tick.
	CurrentChromatic = DashChromaticPeak;
	SetChromaticAberration(CurrentChromatic);

	SetTickEnabled(true);
}

void UkdGameFeedbackComponent::OnInShadowTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (NewCount > 0)
	{
		// Entered shadow plane
		PlayCameraShake(ShadowEntryShakeClass);
		CurrentChromatic = ShadowEntryChromaticPeak;
		SetChromaticAberration(CurrentChromatic);
	}
	else
	{
		// Left shadow plane
		PlayCameraShake(ShadowExitShakeClass);
	}

	SetTickEnabled(true);
}

void UkdGameFeedbackComponent::OnExhaustedTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (NewCount > 0)
	{
		// Stamina just hit zero — big shake
		PlayCameraShake(StaminaEmptyShakeClass);
		SetTickEnabled(true);
	}
}

void UkdGameFeedbackComponent::OnStaminaChanged(const FOnAttributeChangeData& Data)
{
	if (!CachedASC) return;

	const float MaxStam = CachedASC->GetNumericAttribute(UkdAttributeSet::GetMaxShadowStaminaAttribute());

	CurrentStaminaFrac = (MaxStam > 0.f) ? FMath::Clamp(Data.NewValue / MaxStam, 0.f, 1.f) : 1.f;

	// Re-enable tick so the vignette can update this frame.
	if (CurrentStaminaFrac < LowStaminaThreshold)
	{
		SetTickEnabled(true);
	}
}

void UkdGameFeedbackComponent::PlayCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass)
{
	if (!ShakeClass) return;

	AkdMyPlayer* Player = Cast<AkdMyPlayer>(GetOwner());
	if (!Player) return;

	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (!PC) return;

	PC->ClientStartCameraShake(ShakeClass);
}

void UkdGameFeedbackComponent::SetChromaticAberration(float Value)
{
	if (!CachedPPMaterial) return;
	// Material scalar parameter — must match your post-process material exactly.
	CachedPPMaterial->SetScalarParameterValue(TEXT("ChromaticAberration"), Value);
}

void UkdGameFeedbackComponent::SetVignetteIntensity(float Value)
{
	if (!CachedPPMaterial) return;
	CachedPPMaterial->SetScalarParameterValue(TEXT("VignetteIntensity"), Value);
}

void UkdGameFeedbackComponent::SetTickEnabled(bool bEnabled)
{
	PrimaryComponentTick.SetTickFunctionEnable(bEnabled);
}

bool UkdGameFeedbackComponent::NeedsTickThisFrame() const
{
	// Keep ticking if chromatic aberration hasn't decayed yet.
	if (CurrentChromatic > KINDA_SMALL_NUMBER) return true;

	// Keep ticking if vignette is still visible or pulsing.
	if (CurrentVignette > KINDA_SMALL_NUMBER) return true;
	if (CurrentStaminaFrac < LowStaminaThreshold) return true;

	return false;
}
