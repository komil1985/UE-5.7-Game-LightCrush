// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "kd_CrushToggle.generated.h"

class UkdCrushTransitionComponent;
/**
 * 
 */
UCLASS()
class THEPERSPVIEW_API Ukd_CrushToggle : public UGameplayAbility
{
	GENERATED_BODY()

public:
	Ukd_CrushToggle();

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

    UFUNCTION()
    void OnTransitionFinished(bool bNewCrushMode);

    UPROPERTY()
    TObjectPtr<UkdCrushTransitionComponent> CachedTransitionComp;

    UPROPERTY(EditDefaultsOnly, Category = "Crush")
    TSubclassOf<UGameplayEffect> CrushDrainEffect;

    /** Maximum yaw error (degrees) from a cardinal direction to permit entering
    *  Crush.  Exiting Crush always works regardless of camera yaw.
    *
    *  8°  = strict — player must deliberately align the camera.
    *  12° = default sweet spot for mouse input (recommended).
    *  20° = lenient — two cardinals start to bleed into each other near 45°.
    *
    *  Tune in the Blueprint CDO; the resolver reads this at toggle time. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crush|Direction",
        meta = (ClampMin = "1.0", ClampMax = "44.0"))
    float CrushAlignmentToleranceDegrees = 12.f;

private:
    FActiveGameplayEffectHandle ShadowDrainEffectHandle;

    /** Removes the drain effect (if live) and invalidates the handle. */
    void RemoveDrainEffect();
	
};
