// Copyright ASKD Games


#include "AbilitySystem/Abilities/kd_CrushToggle.h"
#include "GameplayTags/kdGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Player/kdMyPlayer.h"
#include "Crush/kdCrushTransitionComponent.h"


Ukd_CrushToggle::Ukd_CrushToggle()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    ActivationBlockedTags.AddTag(FkdGameplayTags::Get().State_Transitioning);
    ActivationBlockedTags.AddTag(FkdGameplayTags::Get().State_Exhausted);
}

void Ukd_CrushToggle::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!ASC) return;

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
    AkdMyPlayer* Player = Cast <AkdMyPlayer>(AvatarActor);
    if (!Player) return;

	CachedTransitionComp = Player->CrushTransitionComponent;
    if (!CachedTransitionComp)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    const FkdGameplayTags& Tags = FkdGameplayTags::Get();

    bool bTargetCrushMode = !ASC->HasMatchingGameplayTag(Tags.State_CrushMode);

    ASC->AddLooseGameplayTag(Tags.State_Transitioning);

    if (!CachedTransitionComp->OnTransitionComplete.Contains(this, TEXT("OnTransitionFinished")))
    {
        CachedTransitionComp->OnTransitionComplete.AddDynamic(this, &Ukd_CrushToggle::OnTransitionFinished);
    }
  
    CachedTransitionComp->StartTransition(bTargetCrushMode);
}

void Ukd_CrushToggle::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void Ukd_CrushToggle::OnTransitionFinished(bool bNewCrushMode)
{
	UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;

    if (ASC)
    {
        const FkdGameplayTags& Tags = FkdGameplayTags::Get();

        // Set final crush mode tag
        if (bNewCrushMode)
            ASC->AddLooseGameplayTag(Tags.State_CrushMode);
        else
            ASC->RemoveLooseGameplayTag(Tags.State_CrushMode);

        // Remove transitioning tag
        ASC->RemoveLooseGameplayTag(Tags.State_Transitioning);
    }

    // Unbind delegate to avoid multiple calls
    if (CachedTransitionComp)
    {
        CachedTransitionComp->OnTransitionComplete.RemoveDynamic(this, &Ukd_CrushToggle::OnTransitionFinished);
    }

    // End ability
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

