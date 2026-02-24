// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "InputActionValue.h"
#include "GameplayEffectTypes.h"
#include "kdMyPlayer.generated.h"


class UCameraComponent;
class USpringArmComponent;
class UkdCrushStateComponent;
class UkdCrushTransitionComponent;
class UkdAbilitySystemComponent;
class UkdAttributeSet;
class UGameplayAbility;
UCLASS()
class THEPERSPVIEW_API AkdMyPlayer : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AkdMyPlayer();

	// -- IAbilitySystemInterface --
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UkdCrushStateComponent> CrushStateComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TObjectPtr<UkdCrushTransitionComponent> CrushTransitionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crush | State")
	bool bIsCrushMode;		// Crush mode state flag

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crush | State")
	bool bIsTransitioning;		// Prevents spamming the toggle button
	UFUNCTION()
	void RequestVerticalMove(const FInputActionValue& Value);

	UFUNCTION()
	void RequestCrushToggle();

	UFUNCTION()
	void OnShadowStaminaChanged(const FOnAttributeChangeData& Data);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTransitionFinished(bool bNewCrushState);

	/* -- GAS Components -- */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UkdAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UkdAttributeSet> AttributeSet;

	/* -- Initial Setup -- */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** Initialize GAS (Attributes, Tags) */
	void InitializeAbilitySystem();

private:
	UPROPERTY(EditAnywhere, Category = "Crush | Movement")
	FVector PlaneConstraintNormal = FVector(1.f, 0.f, 0.f);		// Lock x axis movement

	UPROPERTY(EditAnywhere, Category = "Crush | Movement")
	float PlaneConstraintXValue = 0.f;

	UPROPERTY(EditAnywhere, Category = "Crush | Visuals")
	UMaterialInterface* CrushPostProcessMaterial; // Assign M_CrushOutline here in Blueprint

	// Dynamic instance so we can fade it in/out
	UPROPERTY()
	UMaterialInstanceDynamic* CrushPPInstance;

	// Weighted blendable to control intensity
	FWeightedBlendable CrushBlendable;
};
