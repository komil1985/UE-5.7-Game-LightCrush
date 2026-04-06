// Copyright ASKD Games


#include "Crush/kdCrushStateComponent.h"
#include "Player/kdMyPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DirectionalLight.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "AbilitySystem/kdAbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "AbilitySystem/kdAttributeSet.h"

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
	FindDirectionalLight();

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
	const float MovementSpeed = CachedOwner->GetVelocity().Size2D();
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

	// Drain stamina only when in crush mode and moving
	if (bInCrushMode && bIsMoving)
	{
		StaminaDelta = -StaminaDrainRate * DeltaTime;
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Verbose, TEXT("  -> Draining: %f"), StaminaDelta);
#endif
	}
	else if (!bIsMoving && TimeSinceLastMove >= RegenDelay)
	{
		// Get current and max stamina values
		float CurrentStamina = ASC->GetNumericAttribute(UkdAttributeSet::GetShadowStaminaAttribute());
		float MaxStamina = ASC->GetNumericAttribute(UkdAttributeSet::GetMaxShadowStaminaAttribute());
		// Only regen if below max
		if (CurrentStamina < MaxStamina)
		{
			float DesiredDelta = StaminaRegenRate * DeltaTime;
			StaminaDelta = FMath::Min(DesiredDelta, MaxStamina - CurrentStamina);
#if !UE_BUILD_SHIPPING
			UE_LOG(LogTemp, Verbose, TEXT("  -> Regenerating: %f"), StaminaDelta);
#endif
		}
	}

	// Determine if stamina is currently draining
	bool bIsDraining = (StaminaDelta < 0.0f);
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
			ASC->RemoveLooseGameplayTag(StateTags.State_InShadow);
		//SetComponentTickEnabled(false);
		return;
	}

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
	
	if (!bEnable) ResetPhysicsTo3D();
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
	
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CachedOwner);

	FVector Start = CachedOwner->GetActorLocation();
	FVector End = Start + (CachedLightDirection * ShadowTraceDistance);

	bool bIsHit = GetWorld()->LineTraceSingleByChannel
	(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		Params
	);

#if !UE_BUILD_SHIPPING
	if (bShowDebugLines)
	{
		DrawDebugLine(GetWorld(), Start, bIsHit ? HitResult.ImpactPoint : Start + CachedLightDirection * 1000.0f,
			bIsHit ? FColor::Green : FColor::Red, false, ShadowCheckFrequency * 1.1f, 0, 2.0f);
	}
#endif

	return bIsHit;

}

void UkdCrushStateComponent::FindDirectionalLight()
{
	UWorld* World = GetWorld();
	if (!World)	return;

	// Find the Light Source Actor in the world (assuming there's only one for simplicity)
	if (!DirectionalLightActor)
	{
		TArray<AActor*> FoundLights;
		UGameplayStatics::GetAllActorsOfClass(World, ADirectionalLight::StaticClass(), FoundLights);
		if (FoundLights.Num() > 0)
		{
			DirectionalLightActor = Cast<ADirectionalLight>(FoundLights[0]);
			CachedLightDirection = -DirectionalLightActor->GetActorForwardVector(); // Light direction is opposite to forward vector
		}
		else
		{
			CachedLightDirection = FVector(0, 0, 1); // Default to upward light if no directional light found
		#if !UE_BUILD_SHIPPING
			UE_LOG(LogTemp, Warning, TEXT("CrushStateComponent: No DirectionalLight found!"));
		#endif
		}
	}
}

void UkdCrushStateComponent::ResetPhysicsTo3D()
{
	if (!CachedOwner) return;
	
	UCharacterMovementComponent* MoveComp = CachedOwner->GetCharacterMovement();
	
	MoveComp->SetMovementMode(MOVE_Walking);
	MoveComp->GravityScale = 1.0f;
	MoveComp->MaxWalkSpeed = 600.0f;
	MoveComp->SetPlaneConstraintEnabled(false);
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
