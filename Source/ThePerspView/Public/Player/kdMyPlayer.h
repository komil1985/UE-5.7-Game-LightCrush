// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "InputActionValue.h"
#include "AbilitySystemComponent.h"
#include "kdMyPlayer.generated.h"


class UCameraComponent;
class USpringArmComponent;
class UkdCrushStateComponent;
class UkdCrushTransitionComponent;
class UkdAbilitySystemComponent;
class UkdAttributeSet;
class UGameplayAbility;
class UWidgetComponent;
class UkdStaminaWidget;
class UWidgetComponent;
UCLASS()
class THEPERSPVIEW_API AkdMyPlayer : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AkdMyPlayer();

	// -- IAbilitySystemInterface -- //
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UkdCrushStateComponent> CrushStateComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UkdCrushTransitionComponent> CrushTransitionComponent;
	/*--------------------------------------------------------------------------*/

	// -- User Interface -- //
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly , Category = "UI")
	TSubclassOf<UkdStaminaWidget> StaminaWidgetClass;

	UPROPERTY()
	TObjectPtr<UkdStaminaWidget> StaminaWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UWidgetComponent> StaminaWidgetComponent;

	//UFUNCTION()
	//void OnDrainStateChanged(bool bIsDraining);
	/*--------------------------------------------------------------------------*/

	UFUNCTION()
	void RequestVerticalMove();

	UFUNCTION()
	void RequestCrushToggle();

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
	UMaterialInterface* CrushPostProcessMaterial; // Assign M_CrushOutline here in Blueprint

	// Dynamic instance so we can fade it in/out
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UMaterialInstanceDynamic> CrushPPInstance;

	// Weighted blendable to control intensity
	FWeightedBlendable CrushBlendable;

};
