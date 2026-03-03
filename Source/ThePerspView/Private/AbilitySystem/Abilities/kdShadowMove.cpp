// Copyright ASKD Games


#include "AbilitySystem/Abilities/kdShadowMove.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystem/kdAttributeSet.h"
#include "Crush/kdCrushStateComponent.h"

UkdShadowMove::UkdShadowMove()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    const FkdGameplayTags& StateTags = FkdGameplayTags::Get();
    

    ActivationRequiredTags.AddTag(StateTags.State_CrushMode);
    ActivationRequiredTags.AddTag(StateTags.State_InShadow);

    ActivationBlockedTags.AddTag(StateTags.State_Exhausted);
}

bool UkdShadowMove::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid()) return false;

    // Require crush mode and in-shadow tag
    const FkdGameplayTags& StateTags = FkdGameplayTags::Get();
    if (!ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(StateTags.State_CrushMode)) return false;
    if (!ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(StateTags.State_InShadow)) return false;

    // Check stamina
    const UkdAttributeSet* AttrSet = Cast<UkdAttributeSet>(ActorInfo->AbilitySystemComponent->GetAttributeSet(UkdAttributeSet::StaticClass()));
    if (!AttrSet) return false;

    return AttrSet->GetShadowStamina() > MinStaminaToActivate;

}

void UkdShadowMove::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{    
    // call into player to perform vertical movement (player handles input)
    AActor* Avatar = ActorInfo->AvatarActor.Get();
    AkdMyPlayer* Player = Cast<AkdMyPlayer>(Avatar);

    if (Player && ActorInfo->AbilitySystemComponent.IsValid())
    {
        // Launch the player upwards.
        ActorInfo->AbilitySystemComponent->AddLooseGameplayTag(FkdGameplayTags::Get().Ability_ShadowJump);
        Player->LaunchCharacter(FVector(0.f, 0.f, 650.f), true, true);
    }

    // Apply cost before committing
    if (ShadowMoveCostEffect)
    {
        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(ShadowMoveCostEffect, GetAbilityLevel());
        if (SpecHandle.IsValid())
        {
            ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
        }
    }

    if (!Player || !CommitAbility(Handle, ActorInfo, ActivationInfo)) // consumes cost if any
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // optionally add a gameplay cue for visual feedback here

    // end the ability immediately
	EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

}

void UkdShadowMove::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(FkdGameplayTags::Get().Ability_ShadowJump);
    }
    //Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
