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
#include "Components/SkeletalMeshComponent.h"


// ── Parameter names must match your material exactly ─────────────────────────
static const FName PP_Chroma = TEXT("ChromaticAberration");
static const FName PP_Vignette = TEXT("VignetteIntensity");


UkdGameFeedbackComponent::UkdGameFeedbackComponent()
{
	// Tick disabled at rest — only enabled when post-process values are animating.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

}

// =============================================================================
// BeginPlay — wires everything up
// =============================================================================
void UkdGameFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	AkdMyPlayer* Player = Cast<AkdMyPlayer>(GetOwner());
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("GameFeedbackComponent: Owner is not AkdMyPlayer! Component disabled."));
		return;
	}

	// Cache camera
	CachedCamera = Player->Camera;

	// Cache ASC
	CachedASC = Cast<UkdAbilitySystemComponent>(Player->GetAbilitySystemComponent());
	if (!CachedASC)
	{
		UE_LOG(LogTemp, Error, TEXT("GameFeedbackComponent: Could not find AbilitySystemComponent! Shakes/tags disabled."));
	}

	// ── Create CharMeshDMI for rim glow ──────────────────────────────────────
	// Must happen AFTER the mesh is fully initialized (BeginPlay guarantees this).
	if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* Mesh = Char->GetMesh())
		{
			UMaterialInterface* BaseMat = Mesh->GetMaterial(RimMaterialSlotIndex);
			if (BaseMat)
			{
				CharMeshDMI = UMaterialInstanceDynamic::Create(BaseMat, this);
				Mesh->SetMaterial(RimMaterialSlotIndex, CharMeshDMI);
				// Initialise to zero so there's no flash on first frame
				CharMeshDMI->SetScalarParameterValue(RimIntensityParamName, 0.f);
				UE_LOG(LogTemp, Log, TEXT("GameFeedbackComponent: CharMeshDMI created OK (slot %d)."), RimMaterialSlotIndex);
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("GameFeedbackComponent: No material at RimMaterialSlotIndex=%d on mesh!"), RimMaterialSlotIndex);
			}
		}
	}

	// ── Create the Dynamic Material Instance ourselves ────────────────────────
	// We do this here so nothing external needs to set it up.
	if (TryGetPPInstance())
	{
		// Start fully transparent — no effects until triggered
		WritePP_Chromatic(0.f);
		WritePP_Vignette(0.f);
		WritePP_BlendWeight(0.f); // invisible until crush mode activates

		UE_LOG(LogTemp, Log, TEXT("GameFeedbackComponent: Post-process material instance created OK."));
	}
	else
	{
		UE_LOG(LogTemp, Warning,TEXT("GameFeedbackComponent: CrushPostProcessMaterial not assigned in BP_Player! "
				"Open BP_Player → select GameFeedbackComponent → assign material in Details panel."));
	}

	// ── Subscribe to GAS tags and attributes ─────────────────────────────────
	if (CachedASC)
	{
		const FkdGameplayTags& Tags = FkdGameplayTags::Get();

		CachedASC->RegisterGameplayTagEvent(Tags.State_CrushMode,EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UkdGameFeedbackComponent::OnCrushModeTagChanged);
		CachedASC->RegisterGameplayTagEvent(Tags.State_InShadow,EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UkdGameFeedbackComponent::OnInShadowTagChanged);
		CachedASC->RegisterGameplayTagEvent(Tags.State_Exhausted,EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UkdGameFeedbackComponent::OnExhaustedTagChanged);
		CachedASC->GetGameplayAttributeValueChangeDelegate(UkdAttributeSet::GetShadowStaminaAttribute()).AddUObject(this, &UkdGameFeedbackComponent::OnStaminaChanged);

		// Sync stamina fraction from current value
		const float MaxStam = CachedASC->GetNumericAttribute(UkdAttributeSet::GetMaxShadowStaminaAttribute());
		const float CurStam = CachedASC->GetNumericAttribute(UkdAttributeSet::GetShadowStaminaAttribute());
		CurrentStaminaFrac = (MaxStam > 0.f) ? FMath::Clamp(CurStam / MaxStam, 0.f, 1.f) : 1.f;

		UE_LOG(LogTemp, Log, TEXT("GameFeedbackComponent: GAS subscriptions registered OK."));
	}

	// ── Warn about missing shake classes ────────────────────────────────────
#if !UE_BUILD_SHIPPING
	if (!DashShakeClass)
		UE_LOG(LogTemp, Warning, TEXT("GameFeedbackComponent: DashShakeClass not assigned in BP_Player!"));
	if (!ShadowEntryShakeClass)
		UE_LOG(LogTemp, Warning, TEXT("GameFeedbackComponent: ShadowEntryShakeClass not assigned!"));
	if (!ShadowExitShakeClass)
		UE_LOG(LogTemp, Warning, TEXT("GameFeedbackComponent: ShadowExitShakeClass not assigned!"));
	if (!StaminaEmptyShakeClass)
		UE_LOG(LogTemp, Warning, TEXT("GameFeedbackComponent: StaminaEmptyShakeClass not assigned!"));
#endif
}

// =============================================================================
// Tick — decays chromatic aberration and pulses vignette
// =============================================================================
void UkdGameFeedbackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	//Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//// ── Chromatic aberration: exponential decay toward zero ──────────────────
	//if (CurrentChromatic > KINDA_SMALL_NUMBER)
	//{
	//	CurrentChromatic = FMath::Max(0.f,CurrentChromatic - ChromaticDecaySpeed * DeltaTime);
	//	WritePP_Chromatic(CurrentChromatic);
	//}

	//// ── Vignette: pulse only in crush mode when stamina is low ───────────────
	//if (bInCrushMode && CurrentStaminaFrac < LowStaminaThreshold)
	//{
	//	// Severity: 0 = just crossed threshold, 1 = fully empty
	//	const float Severity = 1.f - (CurrentStaminaFrac / LowStaminaThreshold);
	//	VignettePhase += VignettePulseFrequency * 2.f * PI * DeltaTime;
	//	TargetVignette = MaxVignetteIntensity * Severity * (0.5f + 0.5f * FMath::Sin(VignettePhase));
	//}
	//else
	//{
	//	TargetVignette = 0.f;
	//	if (!bInCrushMode) VignettePhase = 0.f;
	//}

	//const float NewVignette = FMath::FInterpTo(CurrentVignette, TargetVignette, DeltaTime, VignetteLerpSpeed);

	//if (!FMath::IsNearlyEqual(NewVignette, CurrentVignette, 0.001f))
	//{
	//	CurrentVignette = NewVignette;
	//	WritePP_Vignette(CurrentVignette);
	//}

	//// ── Shadow vignette lerp ──────────────────────────────────────────────────
	//if (FMath::Abs(CurrentShadowVignette - TargetShadowVignette) > KINDA_SMALL_NUMBER)
	//{
	//	CurrentShadowVignette = FMath::FInterpTo(CurrentShadowVignette, TargetShadowVignette, DeltaTime, ShadowVignetteLerpSpeed);

	//	// Combine both vignettes — stamina pulse + shadow entry
	//	// WritePP_Vignette already exists; just feed it the combined value
	//	const float Combined = FMath::Clamp(CurrentVignette + CurrentShadowVignette, 0.f, 1.f);
	//	WritePP_Vignette(Combined);
	//}

	//// ── Rim glow lerp ─────────────────────────────────────────────────────────
	//if (FMath::Abs(CurrentRimIntensity - TargetRimIntensity) > KINDA_SMALL_NUMBER)
	//{
	//	CurrentRimIntensity = FMath::FInterpTo(CurrentRimIntensity, TargetRimIntensity, DeltaTime, RimLerpSpeed);
	//	WritePP_Rim(CurrentRimIntensity);
	//}

	//// Auto-disable tick when nothing is animating
	//if (!NeedsTick())
	//{
	//	SetTickActive(false);
	//}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ── Chromatic aberration decay ────────────────────────────────────────────
	if (CurrentChromatic > KINDA_SMALL_NUMBER)
	{
		CurrentChromatic = FMath::Max(0.f, CurrentChromatic - ChromaticDecaySpeed * DeltaTime);
		WritePP_Chromatic(CurrentChromatic);
	}

	// ── Stamina vignette target ───────────────────────────────────────────────
	// Only calculate TargetVignette here — do NOT write to material yet.
	if (bInCrushMode && CurrentStaminaFrac < LowStaminaThreshold)
	{
		const float Severity = 1.f - (CurrentStaminaFrac / LowStaminaThreshold);
		VignettePhase += VignettePulseFrequency * 2.f * PI * DeltaTime;
		TargetVignette = MaxVignetteIntensity * Severity * (0.5f + 0.5f * FMath::Sin(VignettePhase));
	}
	else
	{
		TargetVignette = 0.f;
		if (!bInCrushMode) VignettePhase = 0.f;
	}

	// ── Shadow vignette lerp ──────────────────────────────────────────────────
	CurrentShadowVignette = FMath::FInterpTo(CurrentShadowVignette, TargetShadowVignette, DeltaTime, ShadowVignetteLerpSpeed);

	// ── Stamina vignette lerp ─────────────────────────────────────────────────
	CurrentVignette = FMath::FInterpTo(CurrentVignette, TargetVignette, DeltaTime, VignetteLerpSpeed);

	// ── FIX 2: ONE combined vignette write, always, every tick ───────────────
	// Both sources always summed. No conditional. No second write anywhere.
	const float CombinedVignette = FMath::Clamp(CurrentVignette + CurrentShadowVignette, 0.f, 1.f);
	WritePP_Vignette(CombinedVignette);

	// ── Rim glow lerp ─────────────────────────────────────────────────────────
	if (FMath::Abs(CurrentRimIntensity - TargetRimIntensity) > KINDA_SMALL_NUMBER)
	{
		CurrentRimIntensity = FMath::FInterpTo(CurrentRimIntensity, TargetRimIntensity, DeltaTime, RimLerpSpeed);
		WritePP_Rim(CurrentRimIntensity);
	}

	if (!NeedsTick())
	{
		SetTickActive(false);
	}
}

// =============================================================================
// Public API
// =============================================================================
void UkdGameFeedbackComponent::OnDashPerformed()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("GameFeedbackComponent: OnDashPerformed fired."));
#endif
	PlayShake(DashShakeClass);
	CurrentChromatic = DashChromaticPeak;
	WritePP_Chromatic(CurrentChromatic);
	SetTickActive(true);
}

// =============================================================================
// Tag callbacks
// =============================================================================
void UkdGameFeedbackComponent::OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bInCrushMode = (NewCount > 0);

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("GameFeedbackComponent: CrushMode %s"), bInCrushMode ? TEXT("ENTERED") : TEXT("EXITED"));
#endif

	if (bInCrushMode)
	{
		// Make post-process visible now that we're in crush mode
		WritePP_BlendWeight(1.f);
	}
	else
	{
		// Zero everything out and hide the material
		CurrentChromatic = 0.f;
		CurrentVignette = 0.f;
		TargetVignette = 0.f;
		VignettePhase = 0.f;
		CurrentRimIntensity = 0.0f;
		TargetRimIntensity = 0.f;
		CurrentShadowVignette = 0.f;
		TargetShadowVignette = 0.f;

		WritePP_Chromatic(0.f);
		WritePP_Vignette(0.f);
		WritePP_BlendWeight(0.f);
		WritePP_Rim(0.0f);
	}
}

void UkdGameFeedbackComponent::OnInShadowTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("GameFeedbackComponent: Shadow ENTERED — shake + chroma"));
#endif
		PlayShake(ShadowEntryShakeClass);
		CurrentChromatic = ShadowEntryChromaticPeak;
		WritePP_Chromatic(CurrentChromatic);
	}
	else
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("GameFeedbackComponent: Shadow EXITED — shake"));
#endif
		PlayShake(ShadowExitShakeClass);
	}
	//SetTickActive(true);

	TargetRimIntensity = (NewCount > 0) ? RimPeakIntensity : 0.f;
	TargetShadowVignette = (NewCount > 0) ? InShadowVignetteStrength : 0.f;
	SetTickActive(true);
}

void UkdGameFeedbackComponent::OnExhaustedTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("GameFeedbackComponent: Stamina EXHAUSTED — big shake"));
#endif
		PlayShake(StaminaEmptyShakeClass);
		SetTickActive(true);
	}
}

// =============================================================================
// Attribute callback
// =============================================================================
void UkdGameFeedbackComponent::OnStaminaChanged(const FOnAttributeChangeData& Data)
{
	if (!CachedASC) return;

	const float Max = CachedASC->GetNumericAttribute(UkdAttributeSet::GetMaxShadowStaminaAttribute());
	CurrentStaminaFrac = (Max > 0.f) ? FMath::Clamp(Data.NewValue / Max, 0.f, 1.f) : 1.f;

	// Wake tick so vignette can update
	if (bInCrushMode && CurrentStaminaFrac < LowStaminaThreshold)
	{
		SetTickActive(true);
	}
}

// =============================================================================
// Helpers
// =============================================================================
void UkdGameFeedbackComponent::PlayShake(TSubclassOf<UCameraShakeBase> ShakeClass)
{
	if (!ShakeClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GameFeedbackComponent: Tried to play shake but class not assigned in BP_Player!"));
		return;
	}

	AkdMyPlayer* Player = Cast<AkdMyPlayer>(GetOwner());
	if (!Player) return;

	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GameFeedbackComponent: No PlayerController found — shake skipped."));
		return;
	}

	PC->ClientStartCameraShake(ShakeClass, 1.f);

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("GameFeedbackComponent: Playing shake [%s]"), *ShakeClass->GetName());
#endif
}

bool UkdGameFeedbackComponent::TryGetPPInstance()
{
	// Already created — return immediately
	if (PPInstance) return true;

	// No material assigned — nothing to do
	if (!CrushPostProcessMaterial) return false;

	// Require a camera to add the blendable to
	if (!CachedCamera) return false;

	// Create a unique dynamic instance so we can write scalar params
	PPInstance = UMaterialInstanceDynamic::Create(CrushPostProcessMaterial, this);
	if (!PPInstance) return false;

	// Add to the camera's post-process chain at weight 0 (invisible until needed)
	FWeightedBlendable Blendable;
	Blendable.Weight = 0.f;
	Blendable.Object = PPInstance;
	CachedCamera->PostProcessSettings.WeightedBlendables.Array.Add(Blendable);

	return true;
}

void UkdGameFeedbackComponent::WritePP_Chromatic(float Value)
{
	if (!PPInstance && !TryGetPPInstance()) return;
	PPInstance->SetScalarParameterValue(PP_Chroma, Value);
}

void UkdGameFeedbackComponent::WritePP_Vignette(float Value)
{
	if (!PPInstance && !TryGetPPInstance()) return;
	PPInstance->SetScalarParameterValue(PP_Vignette, Value);
}

void UkdGameFeedbackComponent::WritePP_BlendWeight(float Weight)
{
	if (!CachedCamera) return;
	// Update the weight of our blendable in the camera's array
	for (FWeightedBlendable& Blend : CachedCamera->PostProcessSettings.WeightedBlendables.Array)
	{
		if (Blend.Object == PPInstance)
		{
			Blend.Weight = Weight;
			return;
		}
	}
}

void UkdGameFeedbackComponent::SetTickActive(bool bActive)
{
	PrimaryComponentTick.SetTickFunctionEnable(bActive);
}

bool UkdGameFeedbackComponent::NeedsTick() const
{
	//return bInCrushMode
	//	|| CurrentChromatic > KINDA_SMALL_NUMBER
	//	|| FMath::Abs(CurrentVignette - TargetVignette) > KINDA_SMALL_NUMBER
	//	|| FMath::Abs(CurrentRimIntensity - TargetRimIntensity) > KINDA_SMALL_NUMBER;

	return bInCrushMode
		|| CurrentChromatic > KINDA_SMALL_NUMBER
		|| FMath::Abs(CurrentVignette - TargetVignette) > KINDA_SMALL_NUMBER
		|| FMath::Abs(CurrentShadowVignette - TargetShadowVignette) > KINDA_SMALL_NUMBER
		|| FMath::Abs(CurrentRimIntensity - TargetRimIntensity) > KINDA_SMALL_NUMBER;
}

void UkdGameFeedbackComponent::WritePP_Rim(float Value)
{
	if (!CharMeshDMI) return;
	CharMeshDMI->SetScalarParameterValue(RimIntensityParamName, Value);
}

