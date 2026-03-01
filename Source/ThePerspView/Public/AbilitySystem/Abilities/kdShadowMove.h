// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "kdShadowMove.generated.h"

/**
 * 
 */
UCLASS()
class THEPERSPVIEW_API UkdShadowMove : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
    UkdShadowMove();

    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "ShadowMove")
    float MinStaminaToActivate = 1.0f;          //Stamina threshold to allow activation

	UPROPERTY(EditDefaultsOnly, Category = "ShadowMove")
    TSubclassOf<UGameplayEffect> ShadowMoveCostEffect;  // GameplayEffect that applies the stamina cost of the ability

};
