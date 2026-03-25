// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySystemComponent.h"
#include "kdStaminaManagerComponent.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEPERSPVIEW_API UkdStaminaManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UkdStaminaManagerComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Called by SrushStateComponent when shadow state changes
	void SetInShadow(bool bInShadow);

	// Called by player when movement state changes
	void SetMoving(bool bMoving);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Stamina | Effects")
	TSubclassOf<UGameplayEffect> DrainEffectClass;		// periodic infinite effect

	UPROPERTY(EditDefaultsOnly, Category = "Stamina | Effects")
	TSubclassOf<UGameplayEffect> RegenEffectClass;		// periodic infinite effect

	UPROPERTY(EditDefaultsOnly, Category = "Stamina | Settings")
	float RegenDelay = 2.0f;		// seconds after stopping movement before regen starts

private:
	TObjectPtr<UAbilitySystemComponent> ASC;
	FActiveGameplayEffectHandle DrainEffectHandle;
	FActiveGameplayEffectHandle RegenEffectHandle;
	float TimeSinceLastMove;
	bool bWantsRegen;
	bool bIsInShadow;
	bool bIsMoving;

	void UpdateEffects();
};
