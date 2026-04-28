// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
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
class UkdStaminaWidget;
class UWidgetComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UkdGameFeedbackComponent;
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

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UkdFallDamageComponent> FallDamageComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UkdGameFeedbackComponent> GameFeedbackComponent;
	/*--------------------------------------------------------------------------*/

	// -- User Interface -- //
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly , Category = "UI")
	TSubclassOf<UkdStaminaWidget> StaminaWidgetClass;

	UPROPERTY()
	TObjectPtr<UkdStaminaWidget> StaminaWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UWidgetComponent> StaminaWidgetComponent;
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

	FORCEINLINE UMaterialInstanceDynamic* GetCrushPPInstance() const { return CrushPPInstance; }

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

	UPROPERTY(EditAnywhere, Category = "Crush | Movement")
	float PlaneConstraintXValue = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Crush | Visuals")
	TObjectPtr<UMaterialInterface> CrushPostProcessMaterial;

	// Dynamic instance so we can fade it in/out
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UMaterialInstanceDynamic> CrushPPInstance;

	// Weighted blendable to control intensity
	FWeightedBlendable CrushBlendable;

};
