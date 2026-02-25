// Copyright ASKD Games


#include "AbilitySystem/Abilities/kdShadowMove.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystem/kdAttributeSet.h"

UkdShadowMove::UkdShadowMove()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

bool UkdShadowMove::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
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
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) // consumes cost if any
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Example: call into player to perform vertical movement (player handles input)
    AActor* Avatar = ActorInfo->AvatarActor.Get();
    AkdMyPlayer* Player = Cast<AkdMyPlayer>(Avatar);
    if (Player)
    {
        // Optionally set a tag to indicate ability active
        ActorInfo->AbilitySystemComponent->AddLooseGameplayTag(FkdGameplayTags::Get().Ability_ShadowJump);

        // Let player handle vertical input; ability remains active until stamina exhausted or cancelled
    }
}

void UkdShadowMove::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(FkdGameplayTags::Get().Ability_ShadowJump);
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
