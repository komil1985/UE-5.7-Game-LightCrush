// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "kdShadowDash.generated.h"

/**
 * 
 */
UCLASS()
class THEPERSPVIEW_API UkdShadowDash : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UkdShadowDash();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Dash", meta = (ClampMin = "0.0"))
	float DashStrength = 1400.f;

	/** Instant stamina cost effect. Assign UkdShadowDashCost (or a Blueprint subclass). */
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	TSubclassOf<UGameplayEffect> DashCostEffect;

	/** Duration cooldown effect. Assign UkdShadowDashCooldown (or a Blueprint subclass). */
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	TSubclassOf<UGameplayEffect> DashCooldownEffect;
};
