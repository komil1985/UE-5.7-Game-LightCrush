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
	const bool bInCrushMode = ASC->HasMatchingGameplayTag(StateTags.State_CrushMode);

	if (!bInCrushMode)
	{
		// Ensure normal gravity and exit early
		if (CachedOwner->GetCharacterMovement()->GravityScale != 1.0f)
			CachedOwner->GetCharacterMovement()->GravityScale = 1.0f;
		if(ASC->HasMatchingGameplayTag(StateTags.State_InShadow))
			ASC->RemoveLooseGameplayTag(StateTags.State_InShadow);
		return;
	}

	// Adaptive interval based on movement speed
	const float MovementSpeed = CachedOwner->GetVelocity().Size();
	const bool bIsMoving = MovementSpeed > 1.0f;
	const float DesiredInterval = bIsMoving ? ShadowCheckFrequencyMoving : ShadowCheckFrequency;

	TimeSinceLastShadowCheck += DeltaTime;
	if (TimeSinceLastShadowCheck < DesiredInterval)
		return;
	TimeSinceLastShadowCheck = 0.0f;


	// Perform shadow check
	bIsInShadow = IsStandingInShadow();

	// Apply physics changes based on shadow state
	if (bIsInShadow)
	{
		CachedOwner->GetCharacterMovement()->GravityScale = CrushGravityScale;
		if (!ASC->HasMatchingGameplayTag(StateTags.State_InShadow)) ASC->AddLooseGameplayTag(StateTags.State_InShadow);
	}
	else
	{
		CachedOwner->GetCharacterMovement()->GravityScale = 1.0f;
		if (ASC->HasMatchingGameplayTag(StateTags.State_InShadow)) ASC->RemoveLooseGameplayTag(StateTags.State_InShadow);
	}
}

void UkdCrushStateComponent::ToggleShadowTracking(bool bEnable)
{
	if (!CachedOwner) return;
	
	SetComponentTickEnabled(bEnable);
	if (!bEnable) ResetPhysicsTo3D();
	
}

void UkdCrushStateComponent::HandleVerticalInput(float Value)
{
	if (FMath::IsNearlyZero(Value)) return;

	// Allow upward movement only if in a shadow
	if (CachedOwner && CachedOwner->GetAbilitySystemComponent()->HasMatchingGameplayTag(FkdGameplayTags::Get().State_InShadow))
	{		
		CachedOwner->LaunchCharacter(FVector(0, 0, Value * ShadowMoveSpeed), true, true);
	}
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
	if (CachedOwner)
	{
		CachedOwner->GetCharacterMovement()->GravityScale = 1.0f;
		CachedOwner->GetCharacterMovement()->SetPlaneConstraintEnabled(false);
	}
}
