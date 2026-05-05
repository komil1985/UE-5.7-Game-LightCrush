// Copyright ASKD Games


#include "Crush/kdCrushStateComponent.h"
#include "Player/kdMyPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/RectLight.h"
#include "Components/LightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/SpotLightComponent.h" 
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "AbilitySystem/kdAbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "AbilitySystem/kdAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Crush/kdCrushTransitionComponent.h"



UkdCrushStateComponent::UkdCrushStateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetComponentTickEnabled(true);
	bShowDebugLines = true;	
}

void UkdCrushStateComponent::BeginPlay()
{
	Super::BeginPlay();
	CachedOwner = Cast<AkdMyPlayer>(GetOwner());
	if (UAbilitySystemComponent* ASC = CachedOwner->GetAbilitySystemComponent())
	{
		ASC->RegisterGameplayTagEvent(FkdGameplayTags::Get().State_CrushMode,EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UkdCrushStateComponent::OnCrushModeTagChanged_Regen);
	}
	//FindDirectionalLight();

	DiscoverAndRegisterLights();

}

void UkdCrushStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CachedOwner || !GetWorld()) return;

	// Only run shadow logic when in crush mode or when moving (to detect entering shadows)
	UAbilitySystemComponent* ASC = CachedOwner->GetAbilitySystemComponent();
	if (!ASC) return;

	const FkdGameplayTags& StateTags = FkdGameplayTags::Get();
	if (ASC->HasMatchingGameplayTag(StateTags.State_Transitioning)) return;
	const bool bInCrushMode = ASC->HasMatchingGameplayTag(StateTags.State_CrushMode);

	// Adaptive interval based on movement speed (stamina handling -- always run)
	const float MovementSpeed = CachedOwner->GetVelocity().Size();
	const float MovingThreshold = 10.0f;
	
	if (MovementSpeed > MovingThreshold)
	{
		bWasMoving = true;
	}
	else if (MovementSpeed < MovingThreshold * 0.5f)
	{
		bWasMoving = false;
	}

	const bool bIsMoving = bWasMoving;

	if (bIsMoving)
	{
		TimeSinceLastMove = 0.0f;
	}
	else
	{
		TimeSinceLastMove += DeltaTime;
	}

	float StaminaDelta = 0.0f;

	/*-------- Stamina Drain Logic ------------------------------------------------------*/
	if (bInCrushMode)
	{
		// ── 2D (Crush) mode ─────────────────────────────────────────
		// Drain only when the player is moving.
		if (bIsMoving)
		{
			const bool bInShadow = ASC->HasMatchingGameplayTag(StateTags.State_InShadow);

			// Use the appropriate drain rate: higher in shadow, lower outside.
			const float ActiveDrainRate = bInShadow ? ShadowStaminaDrainRate : BaseStaminaDrainRate;
			StaminaDelta = -ActiveDrainRate * DeltaTime;
		}
		// No regen while Crush Mode is active (even when idle).
	}
	else
	{
		// ── 3D mode ──────────────────────────────────────────────────
		// Always regenerate (moving or idle), as long as stamina isn’t already full.
		//const float CurrentStamina = ASC->GetNumericAttribute(UkdAttributeSet::GetShadowStaminaAttribute());
		//const float MaxStamina = ASC->GetNumericAttribute(UkdAttributeSet::GetMaxShadowStaminaAttribute());

		//if (CurrentStamina < MaxStamina)
		//{
		//	const float DesiredDelta = StaminaRegenRate * DeltaTime;
		//	StaminaDelta = FMath::Min(DesiredDelta, MaxStamina - CurrentStamina);
		//}
	}
	/*-----------------------------------------------------------------------------*/

	// Determine if stamina is currently draining
	const bool bIsDraining = (StaminaDelta < 0.0f);
	if (bIsDraining != bWasDraining)
	{
		bWasDraining = bIsDraining;
		OnDrainStateChanged.Broadcast(bIsDraining);
	}

	//DEBUG
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Verbose, TEXT("Tick: Moving=%d, Speed: %f, TimeSinceLastMove: %f, bInCrushMode: %d"), bIsMoving, MovementSpeed, TimeSinceLastMove, bInCrushMode);
#endif
	if (!FMath::IsNearlyZero(StaminaDelta))
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Verbose, TEXT("  -> Applying delta: %f"), StaminaDelta);
#endif
		ApplyStaminaDelta(StaminaDelta);
	}
	// --- End stamina handling ---

	//Shadow and gravity logic(only in crush mode)
	if (!bInCrushMode)
	{
		// Ensure normal gravity and exit early and reset tag
		auto* MoveComp = CachedOwner->GetCharacterMovement();
		if (MoveComp->GravityScale != 1.0f)
		{
			MoveComp->GravityScale = 1.0f;
			MoveComp->MaxWalkSpeed = 600.0f;
		}
		if (ASC->HasMatchingGameplayTag(StateTags.State_InShadow))
		{
			ASC->RemoveLooseGameplayTag(StateTags.State_InShadow);
		}
		return;
	}

	// Refresh cached light direction every tick so rotating lights move shadows in real time
	//if (DirectionalLightActor) CachedLightDirection = -DirectionalLightActor->GetActorForwardVector();

	// Adaptive interval based on movement speed
	const float DesiredInterval = bIsMoving ? ShadowCheckFrequencyMoving : ShadowCheckFrequency;
	TimeSinceLastShadowCheck += DeltaTime;
	if (TimeSinceLastShadowCheck < DesiredInterval) return;
	TimeSinceLastShadowCheck = 0.0f;

	// Perform shadow check
	bIsInShadow = IsStandingInShadow();

	
	// Apply physics changes based on shadow state
	UCharacterMovementComponent* MoveComp = CachedOwner->GetCharacterMovement();
	if (bIsInShadow)
	{		
		if (!ASC->HasMatchingGameplayTag(StateTags.State_InShadow))
		{
			ASC->AddLooseGameplayTag(StateTags.State_InShadow);

			// ENTER Shadow 2D movement, Switch to custom 2D shadow physics — gravity OFF, free vertical movement
			MoveComp->SetMovementMode(MOVE_Custom, (uint8)ECustomMovementMode::CMOVE_Shadow2D);
			MoveComp->GravityScale = 0.0f;
			MoveComp->MaxFlySpeed = ShadowMoveSpeed;
			MoveComp->MaxWalkSpeed = ShadowMoveSpeed;
		}
	}
	else
	{
		if (ASC->HasMatchingGameplayTag(StateTags.State_InShadow))
		{
			ASC->RemoveLooseGameplayTag(StateTags.State_InShadow);

			// EXIT Shadow 2D movement (but still in crush mode)
			MoveComp->SetMovementMode(MOVE_Walking);
			MoveComp->MaxWalkSpeed = 600.0f;
			MoveComp->GravityScale = 1.0f;
		}
	}
}

void UkdCrushStateComponent::ToggleShadowTracking(bool bEnable)
{
	if (!CachedOwner) return;
	//if (!bEnable) ResetPhysicsTo3D();

	if (!bEnable)
	{
		ResetPhysicsTo3D();
		// Immediately remove the InShadow tag so the tick never sees it stale.
		if (UAbilitySystemComponent* ASC = CachedOwner->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(FkdGameplayTags::Get().State_InShadow);
		}
	}
}

void UkdCrushStateComponent::HandleVerticalInput(float Value)
{
	if (FMath::IsNearlyZero(Value)) return;

	// Allow upward movement only if in a shadow
	//if (CachedOwner && CachedOwner->GetAbilitySystemComponent()->HasMatchingGameplayTag(FkdGameplayTags::Get().State_InShadow))
	//{		
	//	CachedOwner->LaunchCharacter(FVector(0, 0, Value * ShadowJumpPower), false, true);
	//}
}

bool UkdCrushStateComponent::IsStandingInShadow() const
{
	if (!CachedOwner || !GetWorld()) return false;
	if (RegisteredLights.IsEmpty())  return false;

	const FVector PlayerLocation = CachedOwner->GetActorLocation();

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CachedOwner);

	int32 NumRelevantLights = 0;  // Lights that actually reach the player
	int32 NumBlockedLights = 0;  // Of those, how many are occluded

	for (const FkdRegisteredLight& Light : RegisteredLights)
	{
		const FVector LightDir = GetLightDirectionForPlayer(Light, PlayerLocation);

		// ZeroVector = light doesn't affect this position (out of range / cone).
		// Skip it — it neither helps nor hurts the shadow vote.
		if (LightDir.IsNearlyZero()) continue;

		++NumRelevantLights;

		// For directional: trace to max distance.
		// For positional: trace all the way to the light actor (more accurate, no over-shooting).
		const FVector TraceEnd = (Light.Type == EkdLightSourceType::Directional)
			? PlayerLocation + LightDir * ShadowTraceDistance
			: Light.LightActor->GetActorLocation();

		FHitResult HitResult;
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
			HitResult,
			PlayerLocation,
			TraceEnd,
			ECC_Visibility,
			Params
		);

		if (bBlocked) ++NumBlockedLights;

#if !UE_BUILD_SHIPPING
		if (bShowDebugLines)
		{
			DrawDebugLine(
				GetWorld(),
				PlayerLocation,
				bBlocked ? HitResult.ImpactPoint : TraceEnd,
				bBlocked ? FColor::Green : FColor::Red,
				false,
				ShadowCheckFrequency * 1.1f,
				0, 2.0f
			);
		}
#endif
	}

	// Edge case: no lights actually reach the player → not in shadow
	if (NumRelevantLights == 0) return false;

	// Designer-configurable voting rule
	return bRequireAllLightsBlocked
		? (NumBlockedLights == NumRelevantLights)  // ALL lights must be blocked
		: (NumBlockedLights > 0);                 // ANY one blocked is enough

}

void UkdCrushStateComponent::ResetPhysicsTo3D()
{
	if (!CachedOwner) return;

	UCharacterMovementComponent* MoveComp = CachedOwner->GetCharacterMovement();
	if (!MoveComp) return;

	// ── Restore movement properties ───────────────────────────────────────────
	MoveComp->GravityScale = 1.0f;
	MoveComp->MaxWalkSpeed = 600.0f;
	MoveComp->SetPlaneConstraintEnabled(false);

	// Zero X velocity — it was hard-zeroed every frame in shadow mode anyway,
	// but sanitise before handing off to 3D physics.
	FVector ExitVel = MoveComp->Velocity;
	ExitVel.X = 0.f;
	MoveComp->Velocity = ExitVel;
	MoveComp->SetMovementMode(MOVE_Falling);

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log,
		TEXT("CrushStateComponent: ResetPhysicsTo3D → MOVE_Falling at (%.1f, %.1f, %.1f)"),
		CachedOwner->GetActorLocation().X,
		CachedOwner->GetActorLocation().Y,
		CachedOwner->GetActorLocation().Z);
#endif
}

void UkdCrushStateComponent::DiscoverAndRegisterLights()
{
	RegisteredLights.Empty();
	UWorld* World = GetWorld();
	if (!World) return;

	// ── Manual override path ─────────────────────────────────────────────────
	// When the designer explicitly lists lights, skip auto-discovery entirely.
	if (ManualLightOverrides.Num() > 0)
	{
		for (AActor* Actor : ManualLightOverrides)
		{
			FkdRegisteredLight Entry;
			if (Actor && TryBuildLightEntry(Actor, Entry))
			{
				RegisteredLights.Add(Entry);
			}
		}

#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log,
			TEXT("CrushStateComponent: Registered %d manual light(s)."),
			RegisteredLights.Num());
#endif
		return;
	}

	// ── Auto-discovery path ───────────────────────────────────────────────────
	// Iterate each supported UE light class. A single GetAllActorsOfClass pass
	// per class is cheaper than iterating all world actors with RTTI checks.
	static const TArray<UClass*, TFixedAllocator<4>> LightClasses =
	{
		ADirectionalLight::StaticClass(),
		ASpotLight::StaticClass(),   // Spot before Point — USpotLightComponent : UPointLightComponent
		APointLight::StaticClass(),
		ARectLight::StaticClass(),
	};

	for (UClass* LightClass : LightClasses)
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(World, LightClass, Found);
		for (AActor* Actor : Found)
		{
			FkdRegisteredLight Entry;
			if (TryBuildLightEntry(Actor, Entry))
			{
				RegisteredLights.Add(Entry);
			}
		}
	}

#if !UE_BUILD_SHIPPING
	if (RegisteredLights.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("CrushStateComponent: No visible light sources found. Shadow detection disabled."));
	}
	else
	{
		UE_LOG(LogTemp, Log,
			TEXT("CrushStateComponent: Auto-registered %d light source(s)."),
			RegisteredLights.Num());
	}
#endif
}

bool UkdCrushStateComponent::TryBuildLightEntry(AActor* Actor, FkdRegisteredLight& OutEntry) const
{
	if (!Actor) return false;

	// All UE light actors extend ALight which exposes GetLightComponent().
	ALight* LightActor = Cast<ALight>(Actor);
	if (!LightActor) return false;

	ULightComponent* LightComp = Cast<ULightComponent>(LightActor->GetLightComponent());
	if (!LightComp || !LightComp->IsVisible()) return false;

	OutEntry.LightActor = Actor;

	// ── Directional ──────────────────────────────────────────────────────────
	if (Cast<ADirectionalLight>(Actor))
	{
		OutEntry.Type = EkdLightSourceType::Directional;
		OutEntry.AttenuationRadius = 0.f;   // infinite range
		OutEntry.OuterConeAngleDeg = 0.f;
		return true;
	}

	// ── Spot (must precede Point because USpotLightComponent : UPointLightComponent)
	if (USpotLightComponent* SpotComp = Cast<USpotLightComponent>(LightComp))
	{
		OutEntry.Type = EkdLightSourceType::Spot;
		OutEntry.AttenuationRadius = SpotComp->AttenuationRadius;
		OutEntry.OuterConeAngleDeg = SpotComp->OuterConeAngle;
		return true;
	}

	// ── Point and Rect (both use ULocalLightComponent for attenuation radius)
	if (ULocalLightComponent* LocalComp = Cast<ULocalLightComponent>(LightComp))
	{
		OutEntry.Type = Cast<ARectLight>(Actor)
			? EkdLightSourceType::Rect
			: EkdLightSourceType::Point;
		OutEntry.AttenuationRadius = LocalComp->AttenuationRadius;
		OutEntry.OuterConeAngleDeg = 0.f;
		return true;
	}

	return false;  // unsupported light type
}

FVector UkdCrushStateComponent::GetLightDirectionForPlayer(const FkdRegisteredLight& Light, const FVector& PlayerLocation) const
{
	if (!IsValid(Light.LightActor)) return FVector::ZeroVector;

	const FVector LightPos = Light.LightActor->GetActorLocation();

	switch (Light.Type)
	{
		// ── Directional: uniform direction, no attenuation ───────────────────────
	case EkdLightSourceType::Directional:
		// Actor forward vector points AWAY from the light direction in UE convention.
		return -Light.LightActor->GetActorForwardVector();

		// ── Point / Rect: radial, cull by radius ─────────────────────────────────
	case EkdLightSourceType::Point:
	case EkdLightSourceType::Rect:
	{
		if (Light.AttenuationRadius > 0.f &&
			FVector::DistSquared(PlayerLocation, LightPos) > FMath::Square(Light.AttenuationRadius))
		{
			return FVector::ZeroVector;  // Out of range — this light doesn't reach the player
		}
		// Direction: from player UP toward the light (trace goes toward light source)
		return (LightPos - PlayerLocation).GetSafeNormal();
	}

	// ── Spot: radial + cone-angle cull ────────────────────────────────────────
	case EkdLightSourceType::Spot:
	{
		// Radius cull
		if (Light.AttenuationRadius > 0.f &&
			FVector::DistSquared(PlayerLocation, LightPos) > FMath::Square(Light.AttenuationRadius))
		{
			return FVector::ZeroVector;
		}

		// Cone cull — player must be inside the outer cone for the spot to illuminate them
		const FVector SpotForward = Light.LightActor->GetActorForwardVector();
		const FVector ToPlayer = (PlayerLocation - LightPos).GetSafeNormal();
		const float   DotProduct = FVector::DotProduct(SpotForward, ToPlayer);
		const float   CosOuterCone = FMath::Cos(FMath::DegreesToRadians(Light.OuterConeAngleDeg));

		if (DotProduct < CosOuterCone)
		{
			return FVector::ZeroVector;  // Player is outside the spot cone — not illuminated
		}

		return (LightPos - PlayerLocation).GetSafeNormal();
	}

	default:
		return FVector::ZeroVector;
	}
}

void UkdCrushStateComponent::ApplyStaminaDelta(float Delta)
{
	if (!CachedOwner || !StaminaModEffectClass)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("ApplyStaminaDelta: StaminaModEffectClass is not set!"));
#endif
		return;
	}

	UAbilitySystemComponent* ASC = CachedOwner->GetAbilitySystemComponent();
	if (!ASC) return;

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(CachedOwner);

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(StaminaModEffectClass, 1, EffectContext);
	if (SpecHandle.IsValid())
	{
		// Use a SetByCaller tag"
		static FGameplayTag StaminaDeltaTag = FkdGameplayTags::Get().Data_StaminaDelta;														//FGameplayTag::RequestGameplayTag(FName("Data.StaminaDelta"));
		SpecHandle.Data->SetSetByCallerMagnitude(StaminaDeltaTag, Delta);
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void UkdCrushStateComponent::OnCrushModeTagChanged_Regen(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		// Entered Crush Mode — cancel any pending regen immediately
		StopRegen();
	}
	else
	{
		// Exited Crush Mode — schedule regen after delay
		GetWorld()->GetTimerManager().SetTimer(RegenDelayHandle,this,&UkdCrushStateComponent::BeginRegen,RegenDelay,false);
	}
}

void UkdCrushStateComponent::BeginRegen()
{
	if (!CachedOwner) return;

	UAbilitySystemComponent* ASC = CachedOwner->GetAbilitySystemComponent();
	if (!ASC) return;

	// Guard: player might have re-entered Crush Mode during the delay window
	if (ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode)) return;

	bIsRegenerating = true;

	GetWorld()->GetTimerManager().SetTimer(RegenTickHandle, this, &UkdCrushStateComponent::RegenTick, RegenTickInterval, true);   // looping
}

void UkdCrushStateComponent::RegenTick()
{
	if (!CachedOwner) return;

	UAbilitySystemComponent* ASC = CachedOwner->GetAbilitySystemComponent();
	if (!ASC) return;

	// Stop if Crush Mode was re-entered mid-regen (belt-and-suspenders)
	if (ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode))
	{
		StopRegen();
		return;
	}

	const UkdAttributeSet* AttrSet = Cast<UkdAttributeSet>(ASC->GetAttributeSet(UkdAttributeSet::StaticClass()));
	if (!AttrSet) return;

	// Stop ticking once stamina is full — no wasted GAS calls
	if (AttrSet->GetShadowStamina() >= AttrSet->GetMaxShadowStamina())
	{
		StopRegen();
		return;
	}

	// Positive delta → regen. ApplyStaminaDelta already handles GAS + tag clamping
	ApplyStaminaDelta(StaminaRegenRate * RegenTickInterval);
}

void UkdCrushStateComponent::StopRegen()
{
	bIsRegenerating = false;
	GetWorld()->GetTimerManager().ClearTimer(RegenDelayHandle);
	GetWorld()->GetTimerManager().ClearTimer(RegenTickHandle);
}
