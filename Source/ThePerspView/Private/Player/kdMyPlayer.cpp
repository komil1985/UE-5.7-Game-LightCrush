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
#include "AbilitySystem/Abilities/kd_CrushToggle.h"
#include "AbilitySystem/Abilities/kdShadowMove.h"
#include "UI/Widget/kdStaminaWidget.h"
#include "Components/WidgetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Crush/kdStaminaManagerComponent.h"


AkdMyPlayer::AkdMyPlayer()
{
	PrimaryActorTick.bCanEverTick = false;

	/*	--	Components	--	*/
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 1000.0f;
	SpringArm->SetRelativeRotation(FRotator(-30.0f, 0.0f, 0.0f));
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bInheritYaw = true;
	
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	EyeLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftEye"));
	EyeLeft->SetupAttachment(GetMesh());
	EyeLeft->SetRelativeLocation(FVector(150.0f, 50.0f, 50.0f));

	EyeRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightEye"));
	EyeRight->SetupAttachment(GetMesh());
	EyeRight->SetRelativeLocation(FVector(150.0f, -50.0f, 50.0f));	

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
	/*-----------------------------------------------------------------------------------------------------------*/

	/* -- GAS Setup -- */	
	AbilitySystemComponent = CreateDefaultSubobject<UkdAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(false); // Essential for multiplayer, safe for single
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UkdAttributeSet>(TEXT("AttributeSet"));
	/*-----------------------------------------------------------------------------------------------------------*/

	/* -- Widget Components -- */
	StaminaWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("StaminaWidgetComponent"));
	StaminaWidgetComponent->SetupAttachment(GetMesh());
	StaminaWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	StaminaWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
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
	if (CrushTransitionComponent) CrushTransitionComponent->OnTransitionComplete.AddDynamic(this, &AkdMyPlayer::OnTransitionFinished);
	
	// Initialize AbilitySystem
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		InitializeAbilitySystem();

		// Bind to tag changes for CrushMode
		AbilitySystemComponent->RegisterGameplayTagEvent(
			FkdGameplayTags::Get().State_CrushMode,
			EGameplayTagEventType::NewOrRemoved
		).AddUObject(this, &AkdMyPlayer::OnCrushModeTagChanged);
	}

	if (StaminaWidgetComponent && StaminaWidgetClass)
	{
		StaminaWidgetComponent->SetWidgetClass(StaminaWidgetClass);
		// Optional: force widget creation if not already created
		StaminaWidgetComponent->InitWidget();

		UUserWidget* Widget = StaminaWidgetComponent->GetUserWidgetObject();
		if (StaminaWidget = Cast<UkdStaminaWidget>(Widget))
		{
			StaminaWidget->InitializeWithAbilitySystemComponent(AbilitySystemComponent);
		}
	}
}

void AkdMyPlayer::RequestCrushToggle()
{
	if (!AbilitySystemComponent) return;

	// Find the CrushToggle ability class in the DefaultAbilities array
	for (TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass && AbilityClass->IsChildOf(Ukd_CrushToggle::StaticClass()))
		{
			FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass);
			if (Spec)
			{
				bool bActivated = AbilitySystemComponent->TryActivateAbility(Spec->Handle);
				UE_LOG(LogTemp, Log, TEXT("RequestCrushToggle: TryActivateAbility returned %d"), bActivated);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("RequestCrushToggle: Could not find ability spec for class %s"), *AbilityClass->GetName());
			}
			return;
		}
	}
}

void AkdMyPlayer::OnTransitionFinished(bool bNewCrushState)
{
	if (!AbilitySystemComponent) return;

	// Set Plane constraints for 2D movement
	if (bNewCrushState)
	{
		// Align Player to (X) axis
		FVector NewLocation = GetActorLocation();
		NewLocation.X = PlaneConstraintXValue;
		SetActorLocation(NewLocation);

		GetCharacterMovement()->SetPlaneConstraintNormal(PlaneConstraintNormal);
		GetCharacterMovement()->SetPlaneConstraintEnabled(true);
	}
	else
	{
		GetCharacterMovement()->SetPlaneConstraintEnabled(false);
	}
	// Tell the physics engine to start/stop tracking shadows
	if (CrushStateComponent) CrushStateComponent->ToggleShadowTracking(bNewCrushState);
}

void AkdMyPlayer::OnCrushModeTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (AbilitySystemComponent->HasMatchingGameplayTag(FkdGameplayTags::Get().State_Transitioning))
	{
		return;
	}

	if (NewCount == 0)
	{
		// Crush mode was removed – revert to 3D
		if (CrushTransitionComponent)
		{
			CrushTransitionComponent->StartTransition(false);
		}
		// Also ensure physics are reset (ToggleShadowTracking will be called when transition finishes)
		// Optionally, you can force an immediate reset here if needed, but better to let the transition handle it.
		
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

	// List all granted abilities after giving them
	const TArray<FGameplayAbilitySpec>& Specs = AbilitySystemComponent->GetActivatableAbilities();
	for (const FGameplayAbilitySpec& Spec : Specs)
	{
		if (Spec.Ability)
		{
			 //Check if this ability has the LightCrush tag
			const FGameplayTagContainer& AbilityTags = Spec.Ability->AbilityTags;
		}
	}

	// Initialize attributes with default values
	if (AttributeSet)
	{
		AbilitySystemComponent->GetSpawnedAttributes_Mutable().Add(AttributeSet);

		// Set initial stamina values
		AttributeSet->SetMaxShadowStamina(100.0f);
		AttributeSet->SetShadowStamina(100.0f);
	}
}

void AkdMyPlayer::RequestVerticalMove()
{
	if (!AbilitySystemComponent) return;

	// Find the CrushToggle ability class in the DefaultAbilities array
	for (TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass && AbilityClass->IsChildOf(UkdShadowMove::StaticClass()))
		{
			AbilitySystemComponent->TryActivateAbilityByClass(AbilityClass);
			return;
		}
	}
}
