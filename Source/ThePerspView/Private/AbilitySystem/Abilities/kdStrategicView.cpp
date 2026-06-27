// Copyright ASKD Games. All Rights Reserved.

#include "AbilitySystem/Abilities/kdStrategicView.h"
#include "Components/kdStrategicCameraComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"


UkdStrategicView::UkdStrategicView()
{
	// One persistent instance per actor — matches Ukd_CrushToggle / UkdShadowDash.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	const FkdGameplayTags& Tags = FkdGameplayTags::Get();

	// Identity tag so the ability can be found / cancelled by tag if ever needed.
	if (Tags.Ability_StrategicView.IsValid())
	{
		AbilityTags.AddTag(Tags.Ability_StrategicView);
	}

	// Never OPEN the survey in 2D or mid-morph. (The running-ability case — a crush
	// that starts while already surveying — is handled by OnBlockingStateChanged,
	// because ActivationBlockedTags only gate activation, not active abilities.)
	ActivationBlockedTags.AddTag(Tags.State_CrushMode);
	ActivationBlockedTags.AddTag(Tags.State_Transitioning);
}

UkdStrategicCameraComponent* UkdStrategicView::GetStrategicCamera() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	return IsValid(Avatar) ? Avatar->FindComponentByClass<UkdStrategicCameraComponent>() : nullptr;
}

void UkdStrategicView::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UkdStrategicCameraComponent* Cam = GetStrategicCamera();
	if (!IsValid(Cam))
	{
		// No rig on this avatar — fail clean.
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
		return;
	}

	// Engage the survey. It holds until the controller cancels on input release,
	// or until a crush transition forces an early return below.
	Cam->RequestStrategicView(true);

	// Self-cancel the instant a crush transition begins. State.Transitioning is the
	// first tag crush adds (before the morph), so returning the camera here keeps the
	// overview strictly 3D-only. Same tag-event idiom used across the codebase.
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		BlockingStateHandle = ASC->RegisterGameplayTagEvent(
			FkdGameplayTags::Get().State_Transitioning,
			EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UkdStrategicView::OnBlockingStateChanged);
	}

	// NOTE: intentionally no EndAbility here — this is a held ability.
}

void UkdStrategicView::OnBlockingStateChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
	if (NewCount > 0)
	{
		// A crush transition just started — cancel so EndAbility returns the camera.
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateCancelAbility*/ true);
	}
}

void UkdStrategicView::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Guarantee the camera returns on EVERY exit path: input release, crush cancel,
	// death (CancelAllAbilities), or any other cancellation. The component owns its
	// own return lerp, so this is correct even after the ability instance is gone.
	if (UkdStrategicCameraComponent* Cam = GetStrategicCamera())
	{
		Cam->RequestStrategicView(false);
	}

	// Stop watching the blocking state.
	if (BlockingStateHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			ASC->RegisterGameplayTagEvent(
				FkdGameplayTags::Get().State_Transitioning,
				EGameplayTagEventType::NewOrRemoved).Remove(BlockingStateHandle);
		}
		BlockingStateHandle.Reset();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
