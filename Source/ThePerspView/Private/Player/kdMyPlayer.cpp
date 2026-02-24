// Copyright ASKD Games


#include "Player/kdMyPlayer.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Crush/kdCrushStateComponent.h"
#include "Crush/kdCrushTransitionComponent.h"
#include "AbilitySystem/kdAbilitySystemComponent.h"
#include "AbilitySystem/kdAttributeSet.h"
#include "GameplayTags/kdGameplayTags.h"

AkdMyPlayer::AkdMyPlayer()
{
	PrimaryActorTick.bCanEverTick = false;

	/*	--	Components	--	*/
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 500.0f;
	SpringArm->SetRelativeRotation(FRotator(-30.0f, 0.0f, 0.0f));
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bInheritYaw = true;
	
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	CrushStateComponent = CreateDefaultSubobject<UkdCrushStateComponent>(TEXT("CrushState"));
	CrushTransitionComponent = CreateDefaultSubobject<UkdCrushTransitionComponent>(TEXT("CrushTransition"));
	/*-----------------------------------------------------------------------------------------------------------*/

	/*	--	Default Values	--	*/
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	bIsCrushMode = false;
	bIsTransitioning = false;
	/*-----------------------------------------------------------------------------------------------------------*/

	/* -- GAS Setup -- */	
	AbilitySystemComponent = CreateDefaultSubobject<UkdAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true); // Essential for multiplayer, safe for single
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UkdAttributeSet>(TEXT("AttributeSet"));
	/*-----------------------------------------------------------------------------------------------------------*/

}

UAbilitySystemComponent* AkdMyPlayer::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AkdMyPlayer::BeginPlay()
{
	Super::BeginPlay();

	// Binding Transition Finished Event
	if (CrushTransitionComponent)	CrushTransitionComponent->OnTransitionComplete.AddDynamic(this, &AkdMyPlayer::OnTransitionFinished);
	
	// Initialize GAS
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UkdAttributeSet::GetShadowStaminaAttribute()).AddUObject(this, &AkdMyPlayer::OnShadowStaminaChanged);
		InitializeAbilitySystem();
	}

}

void AkdMyPlayer::RequestCrushToggle()
{
	if (bIsTransitioning) return;

	bIsTransitioning = true;
	bool bTargetState = !bIsCrushMode;

	// Start Visuals
	if (CrushTransitionComponent)	CrushTransitionComponent->StartTransition(bTargetState);
	
}

void AkdMyPlayer::OnShadowStaminaChanged(const FOnAttributeChangeData& Data)
{
	float NewValue = Data.NewValue;

	// If stamina <= 0, add Exhausted tag to ASC to block ability activation
	if (AbilitySystemComponent)
	{
		if (NewValue <= 0.0f)
		{
			AbilitySystemComponent->AddLooseGameplayTag(FkdGameplayTags::Get().State_Exhausted);
		}
		else
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(FkdGameplayTags::Get().State_Exhausted);
		}
	}
}

void AkdMyPlayer::OnTransitionFinished(bool bNewCrushState)
{
	bIsCrushMode = bNewCrushState;
	bIsTransitioning = false;

	// Tell the physics engine to start/stop tracking shadows
	if (CrushStateComponent)	CrushStateComponent->ToggleShadowTracking(bIsCrushMode);
	

	// Set Plane constraints for 2D movement
	if (bIsCrushMode)
	{
		GetCharacterMovement()->SetPlaneConstraintNormal(PlaneConstraintNormal);
		GetCharacterMovement()->SetPlaneConstraintEnabled(true);

		// Align Player to (X) axis
		FVector NewLocation = GetActorLocation();
		NewLocation.X = PlaneConstraintXValue;
		SetActorLocation(NewLocation);
	}
	else
	{
		GetCharacterMovement()->SetPlaneConstraintEnabled(false);
	}
}

void AkdMyPlayer::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent) return;

	// Grant startup abilities (Crush, Jump, etc.)
	for (TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass)
		{
			FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
			AbilitySystemComponent->GiveAbility(Spec);
		}
	}
}

void AkdMyPlayer::RequestVerticalMove(const FInputActionValue& Value)
{
	if (CrushStateComponent)
	{
		CrushStateComponent->HandleVerticalInput(Value.Get<float>());
	}
}
