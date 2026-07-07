// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Crush/kdCrushDirection.h"
#include "kdMyPlayer.generated.h"

UENUM(BlueprintType)
enum class ECustomMovementMode : uint8
{
	CMOVE_None UMETA(DisplayName = "None"),
	CMOVE_Shadow2D UMETA(DisplayName = "Shadow 2D"),
};


class UCameraComponent;
class USpringArmComponent;
class UkdCrushStateComponent;
class UkdCrushTransitionComponent;
class UkdAbilitySystemComponent;
class UkdFallDamageComponent;
class UkdDeathComponent;
class UkdAttributeSet;
class UGameplayAbility;
class UWidgetComponent;
//class UkdStaminaWidget;
class UWidgetComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UkdGameFeedbackComponent;
class UkdLowStaminaWidget;
class UkdPlayerHoverComponent;
class UkdLightHealthComponent;
class UkdLightHealthWidget;
class UkdStrategicCameraComponent;
class UkdJumpSquashComponent;
class UNiagaraComponent;
UCLASS()
class THEPERSPVIEW_API AkdMyPlayer : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AkdMyPlayer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// -- IAbilitySystemInterface -- //
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	/*--------------------------------------------------------------------------*/

	// -- Components -- //
	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UkdCrushStateComponent> CrushStateComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UkdCrushTransitionComponent> CrushTransitionComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UkdDeathComponent> DeathComponent;

	UPROPERTY(EditAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> EyeLeft;

	UPROPERTY(EditAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> EyeRight;

	UPROPERTY(EditAnywhere, Category = "Components")
	TObjectPtr<UNiagaraComponent> Tentacle_1;

	UPROPERTY(EditAnywhere, Category = "Components")
	TObjectPtr<UNiagaraComponent> Tentacle_2;

	UPROPERTY(EditAnywhere, Category = "Components")
	TObjectPtr<UNiagaraComponent> Tentacle_3;

	UPROPERTY(EditAnywhere, Category = "Components")
	TObjectPtr<UNiagaraComponent> Tentacle_4;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UkdFallDamageComponent> FallDamageComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UkdGameFeedbackComponent> GameFeedbackComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UkdPlayerHoverComponent> HoverComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UkdLightHealthComponent> LightHealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UkdStrategicCameraComponent> StrategicCameraComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UkdJumpSquashComponent> JumpSquashComponent;
	/*--------------------------------------------------------------------------*/

	// -- User Interface -- //
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly , Category = "UI")
	//TSubclassOf<UkdStaminaWidget> StaminaWidgetClass;

	//UPROPERTY()
	//TObjectPtr<UkdStaminaWidget> StaminaWidget;

	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	//TObjectPtr<UWidgetComponent> StaminaWidgetComponent;

	// -- Low Stamina Warning Widget -------------------------------------------- //

	/** Blueprint widget class for the world-space "LOW STAMINA!" popup.
	*  Assign WBP_LowStaminaWarning in the Details panel of BP_Player. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UkdLowStaminaWidget> LowStaminaWidgetClass;

	/** Cached instance — created at BeginPlay from LowStaminaWidgetClass. */
	UPROPERTY()
	TObjectPtr<UkdLowStaminaWidget> LowStaminaWidget;

	/** World-space widget component — positioned above the stamina bar. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UWidgetComponent> LowStaminaWidgetComponent;

	/** Blueprint widget class for the light-health bar. Assign WBP_LightHealth in Details. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UkdLightHealthWidget> LightHealthWidgetClass;

	/** Cached widget instance — created at BeginPlay from LightHealthWidgetClass. */
	UPROPERTY()
	TObjectPtr<UkdLightHealthWidget> LightHealthWidget;

	///** World-space widget component that hosts the light health bar. */
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	//TObjectPtr<UWidgetComponent> LightHealthWidgetComponent;
	/*--------------------------------------------------------------------------*/

	/** Called by the controller on Dash input. Finds and activates UkdCrushToggle. */
	UFUNCTION()
	void RequestCrushToggle();

	/** Called by the controller on Interact input. Sphere-overlaps for IkdInteractable actors. */
	UFUNCTION()
	void RequestInteract();

	/** Called by the controller on Dash input. Finds and activates UkdShadowDash. */
	UFUNCTION()
	void RequestShadowDash();

	/** Controller calls these on Strategic-View input press / release. */
	UFUNCTION()
	void RequestStrategicViewStart();

	UFUNCTION()
	void RequestStrategicViewStop();


	FORCEINLINE UMaterialInstanceDynamic* GetCrushPPInstance() const { return CrushPPInstance; }

	// ADD in the public section (near RequestCrushToggle)
	/** The crush axis resolved at the last enter. Every axis-aware system reads
	*  this so they all agree on which way the world collapsed. */
	UFUNCTION(BlueprintPure, Category = "Crush")
	EkdCrushDirection GetActiveCrushDirection() const { return ActiveCrushDirection; }

	void SetActiveCrushDirection(EkdCrushDirection InDirection) { ActiveCrushDirection = InDirection; }

	/** GameMode calls this on level clear. Freezes all survival systems so they
	*  stop running behind the results screen. */
	void HandleLevelComplete();

	UkdGameFeedbackComponent* GetGameFeedbackComponent() const { return GameFeedbackComponent; }

	virtual void NotifyJumpApex() override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTransitionFinished(bool bNewCrushState);

	UFUNCTION()
	void OnCrushModeTagChanged(const FGameplayTag CallbackTag, int32 NewCount);


	/* -- GAS Components -- */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = true))
	TObjectPtr<UkdAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UkdAttributeSet> AttributeSet;
	/*--------------------------------------------------------------------------*/

	/* -- Initial Setup -- */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TSubclassOf<UGameplayEffect> RegenEffectClass;
	/*--------------------------------------------------------------------------*/

	/** Initialize GAS (Attributes, Tags) */
	void InitializeAbilitySystem();
	/*--------------------------------------------------------------------------*/

private:
	UPROPERTY(EditAnywhere, Category = "Crush | Movement")
	FVector PlaneConstraintNormal = FVector(1.f, 0.f, 0.f);		// Lock x axis movement

	//UPROPERTY(EditAnywhere, Category = "Crush | Movement")
	//float PlaneConstraintXValue = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Crush | Visuals")
	TObjectPtr<UMaterialInterface> CrushPostProcessMaterial;

	// Dynamic instance so we can fade it in/out
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UMaterialInstanceDynamic> CrushPPInstance;

	// Weighted blendable to control intensity
	FWeightedBlendable CrushBlendable;

	/** Triggers the "LOW STAMINA!" world-space flash above the player's head. */
	void NotifyLowStaminaWarning();

	EkdCrushDirection ActiveCrushDirection = EkdCrushDirection::PosX;

};
