// Copyright ASKD Games


#include "AbilitySystem/Abilities/kdShadowMove.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystem/kdAttributeSet.h"

UkdShadowMove::UkdShadowMove()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    const FkdGameplayTags& Tags = FkdGameplayTags::Get();

    ActivationRequiredTags.AddTag(Tags.State_CrushMode);
    ActivationRequiredTags.AddTag(Tags.State_InShadow);

    ActivationBlockedTags.AddTag(Tags.State_Exhausted);
}

bool UkdShadowMove::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    UE_LOG(LogTemp, Log, TEXT("ShadowMove CanActivateAbility called"));
    if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid()) return false;

    // Require crush mode and in-shadow tag
    const FkdGameplayTags& Tags = FkdGameplayTags::Get();
    if (!ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(Tags.State_CrushMode)) return false;
    if (!ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(Tags.State_InShadow)) return false;

    // Check stamina
    const UkdAttributeSet* AttrSet = Cast<UkdAttributeSet>(ActorInfo->AbilitySystemComponent->GetAttributeSet(UkdAttributeSet::StaticClass()));
    if (!AttrSet) return false;

    return AttrSet->GetShadowStamina() > MinStaminaToActivate;

}

void UkdShadowMove::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    // Apply cost before committing
    if (ShadowMoveCostEffect)
    {
        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(ShadowMoveCostEffect, GetAbilityLevel());
        if (SpecHandle.IsValid())
        {
            ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
        }
    }

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) // consumes cost if any
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Example: call into player to perform vertical movement (player handles input)
    AActor* Avatar = ActorInfo->AvatarActor.Get();
    AkdMyPlayer* Player = Cast<AkdMyPlayer>(Avatar);
    if (!Player)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
    }
    
    // Launch the player upwards.
    Player->LaunchCharacter(FVector(0.f, 0.f, 800.f), false, true);

    // optionally add a gameplay cue for visual feedback here

    // end the ability immediately
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);

}

void UkdShadowMove::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(FkdGameplayTags::Get().Ability_ShadowJump);
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
