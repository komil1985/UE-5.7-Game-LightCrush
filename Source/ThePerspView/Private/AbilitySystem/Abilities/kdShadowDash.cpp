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

	// Cannot dash while stamina-depleted or already in cooldown or dead
	ActivationBlockedTags.AddTag(Tags.State_Exhausted);
	ActivationBlockedTags.AddTag(Tags.State_Dashing);
	ActivationBlockedTags.AddTag(Tags.State_Dead);

	// Wire up the C++ effect classes as defaults.
	// Override in a Blueprint-derived class if you want to swap effects.
	CostGameplayEffectClass = UkdShadowDashCost::StaticClass();
	CooldownGameplayEffectClass = UkdShadowDashCooldown::StaticClass();
}

void UkdShadowDash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
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

	// ── Bail BEFORE committing cost/cooldown if there's no direction to dash in.
	// An idle press must be a true no-op: no stamina spent, no cooldown started,
	// and (the bug you hit) no phantom dash FX.
	if (!MoveComp->HasDashDirection())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, /*bWasCancelled*/ true);
		return;
	}

	// CommitAbility: re-checks CanActivate, then applies cost + cooldown.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Apply the burst. Guaranteed a direction after the check above, but respect
	// the return value defensively in case state changed between check and commit.
	if (!MoveComp->ApplyShadowDashImpulse(DashStrength))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("ShadowDash: impulse applied, strength=%.0f"), DashStrength);
#endif

	// Fresh, authoritative direction — only reached on a real dash, so FX never
	// fires phantom and never uses a stale direction.
	const FVector DashDir2D = MoveComp->GetActiveDashDirection();

	if (UkdGameFeedbackComponent* GF = Player->FindComponentByClass<UkdGameFeedbackComponent>())
	{
		GF->OnDashPerformed(DashDir2D);
	}

	// Fire-and-forget — end immediately so the ability slot is free.
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UkdShadowDash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
