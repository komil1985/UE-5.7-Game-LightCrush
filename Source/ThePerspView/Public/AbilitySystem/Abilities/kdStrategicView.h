// Copyright ASKD Games
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "kdStrategicView.generated.h"

/**
 * UkdStrategicView
 * ---------------------------------------------------------------------------
 * Hold-to-survey ability. While the bound input is held, the player's
 * UkdStrategicCameraComponent dollies the telephoto rig back into the
 * "Surveyor's Plate" overview; on release it returns to the gameplay baseline.
 *
 * RESPONSIBILITIES (kept intentionally thin)
 *  - Input lifetime: activate on press, end on release (via WaitInputRelease).
 *  - Gating: ActivationBlockedTags ensure the survey can never open during a
 *    crush or a geometry transition. If a crush tag is applied while engaged,
 *    GAS cancels this ability and EndAbility guarantees a graceful return.
 *
 * The per-frame camera lerp lives entirely in UkdStrategicCameraComponent; this
 * ability never ticks. NetExecutionPolicy is LocalOnly because the survey is a
 * purely local, cosmetic camera change with no gameplay-state side effects.
 *
 * SETUP
 *  - Grant this ability to the player's ASC and bind it to a held input action
 *    using your existing GAS input pipeline.
 *  - The gating tags below are requested by name with ErrorIfNotFound = false so
 *    this compiles cleanly; swap them for your native tag accessors if you keep
 *    a centralized FkdGameplayTags singleton.
 */
UCLASS()
class THEPERSPVIEW_API UkdStrategicView : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UkdStrategicView();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	/**
	* Bound (AddUObject, not dynamic) to the State.Transitioning tag event. When a
	* crush transition starts, this cancels the ability so EndAbility returns the
	* camera before the world flattens.
	*/
	void OnBlockingStateChanged(const FGameplayTag Tag, int32 NewCount);

private:
	/** Resolve the strategic camera component from the current avatar, or null. */
	class UkdStrategicCameraComponent* GetStrategicCamera() const;

	/** Handle for the State.Transitioning watcher, removed in EndAbility. */
	FDelegateHandle BlockingStateHandle;
};
