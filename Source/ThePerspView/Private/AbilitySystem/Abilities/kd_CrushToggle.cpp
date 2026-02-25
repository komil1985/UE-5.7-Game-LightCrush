// Copyright ASKD Games


#include "AbilitySystem/Abilities/kd_CrushToggle.h"
#include "GameplayTags/kdGameplayTags.h"
#include "AbilitySystemComponent.h"


Ukd_CrushToggle::Ukd_CrushToggle()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    ActivationBlockedTags.AddTag(FkdGameplayTags::Get().State_Transitioning);
    ActivationBlockedTags.AddTag(FkdGameplayTags::Get().State_Exhausted);
}

void Ukd_CrushToggle::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
        return;

    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!ASC) return;

    const FkdGameplayTags& Tags = FkdGameplayTags::Get();

    bool bIsCrushActive = ASC->HasMatchingGameplayTag(Tags.State_CrushMode);

    ASC->AddLooseGameplayTag(Tags.State_Transitioning);

    if (!bIsCrushActive)
    {
        ASC->AddLooseGameplayTag(Tags.State_CrushMode);
    }
    else
    {
        ASC->RemoveLooseGameplayTag(Tags.State_CrushMode);
    }

    ASC->RemoveLooseGameplayTag(Tags.State_Transitioning);

    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void Ukd_CrushToggle::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

