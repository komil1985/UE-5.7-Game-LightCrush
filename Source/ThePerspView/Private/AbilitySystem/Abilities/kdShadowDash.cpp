// Copyright ASKD Games


#include "AbilitySystem/Abilities/kdShadowDash.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Player/kdMyPlayer.h"
#include "Components/kdCharacterMovementComponent.h"
#include "AbilitySystem/Effects/kdShadowDashCooldown.h"
#include "AbilitySystem/Effects/kdShadowDashCost.h"
#include "Components/kdGameFeedbackComponent.h"

UkdShadowDash::UkdShadowDash()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	const FkdGameplayTags& Tags = FkdGameplayTags::Get();

	AbilityTags.AddTag(Tags.Ability_ShadowDash);

	// Only valid while in 2D shadow mode
	ActivationRequiredTags.AddTag(Tags.State_CrushMode);
	ActivationRequiredTags.AddTag(Tags.State_InShadow);

	// Cannot dash while stamina-depleted or already in cooldown
	ActivationBlockedTags.AddTag(Tags.State_Exhausted);
	ActivationBlockedTags.AddTag(Tags.State_Dashing);

	// Wire up the C++ effect classes as defaults.
	// Override in a Blueprint-derived class if you want to swap effects.
	CostGameplayEffectClass = UkdShadowDashCost::StaticClass();
	CooldownGameplayEffectClass = UkdShadowDashCooldown::StaticClass();
}

void UkdShadowDash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	/* CommitAbility: checks CanActivate, then applies cost + cooldown via the
	 CostGameplayEffectClass / CooldownGameplayEffectClass set in the constructor.*/
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AkdMyPlayer* Player = Cast<AkdMyPlayer>(ActorInfo->AvatarActor.Get());
	if (!Player)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UkdCharacterMovementComponent* MoveComp = Cast<UkdCharacterMovementComponent>(Player->GetCharacterMovement());
	if (!MoveComp)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Apply the burst. The movement component uses its cached last-input direction
	// so the dash always fires the way the player was last steering.
	MoveComp->ApplyShadowDashImpulse(DashStrength);

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("ShadowDash: impulse applied, strength=%.0f"), DashStrength);
#endif

	   // Notify game feel component — triggers shake + aberration spike
   if (UkdGameFeedbackComponent* GF = Player->FindComponentByClass<UkdGameFeedbackComponent>())
   {
       GF->OnDashPerformed();
   }

	// Dash is fire-and-forget — end immediately so the ability slot is free
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UkdShadowDash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
