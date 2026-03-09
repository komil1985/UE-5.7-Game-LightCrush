// Copyright ASKD Games


#include "AbilitySystem/Abilities/kd_CrushToggle.h"
#include "GameplayTags/kdGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Player/kdMyPlayer.h"
#include "Crush/kdCrushTransitionComponent.h"
#include "Crush/kdCrushStateComponent.h"


Ukd_CrushToggle::Ukd_CrushToggle()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // Add the ability tag so it can be found by tag activation
    //FGameplayTag AbilityTag = FkdGameplayTags::Get().Ability_LightCrush;
    //if (AbilityTag.IsValid())
    //{
    //    AbilityTags.AddTag(AbilityTag);
    //}

	const FkdGameplayTags& Tags = FkdGameplayTags::Get();
    if (Tags.Ability_LightCrush.IsValid())
    {
        AbilityTags.AddTag(Tags.Ability_LightCrush);
    }

    ActivationBlockedTags.AddTag(Tags.State_Transitioning);
    ActivationBlockedTags.AddTag(Tags.State_Exhausted);

}

void Ukd_CrushToggle::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("CrushToggle ActivateAbility called"));

    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        FGameplayTagContainer Tags;
        ActorInfo->AbilitySystemComponent->GetOwnedGameplayTags(Tags);
        UE_LOG(LogTemp, Log, TEXT("Current tags: %s"), *Tags.ToStringSimple());
    }
#endif

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode) && CrushDrainEffect)
    {
        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CrushDrainEffect, GetAbilityLevel());
        if (SpecHandle.IsValid())
        {
            // Store the active effect handle so we can remove it later
            FActiveGameplayEffectHandle DrainEffectHandle = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
            // Store this handle in a member variable or tag for later removal
            // For simplicity, we can add a tag and later remove all effects with that tag.
        }
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

    //// --- Apply drain effect when entering crush mode ---
    //if (bTargetCrushMode && CrushDrainEffect)
    //{
    //    FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CrushDrainEffect, GetAbilityLevel());
    //    if (SpecHandle.IsValid())
    //    {
    //        ShadowDrainEffectHandle = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
    //    }
    //}

    ASC->AddLooseGameplayTag(Tags.State_Transitioning);

    if (!CachedTransitionComp->OnTransitionComplete.Contains(this, TEXT("OnTransitionFinished")))
    {
        CachedTransitionComp->OnTransitionComplete.AddDynamic(this, &Ukd_CrushToggle::OnTransitionFinished);
    }
  
    CachedTransitionComp->StartTransition(bTargetCrushMode);
}

void Ukd_CrushToggle::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
		const FkdGameplayTags& Tags = FkdGameplayTags::Get();
        if (ASC->HasMatchingGameplayTag(Tags.State_Transitioning))
        {
            ASC->RemoveLooseGameplayTag(Tags.State_Transitioning);
        }
    }

    if (CachedTransitionComp)
    {
        CachedTransitionComp->OnTransitionComplete.RemoveDynamic(this, &Ukd_CrushToggle::OnTransitionFinished);
    }

    // Optionally remove drain effect if still active
    //if (ShadowDrainEffectHandle.IsValid())
    //{
    //    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    //    {
    //        ASC->RemoveActiveGameplayEffect(ShadowDrainEffectHandle);
    //    }
    //    ShadowDrainEffectHandle.Invalidate();
    //}

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void Ukd_CrushToggle::OnTransitionFinished(bool bNewCrushMode)
{
	UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;

    if (ASC)
    {
        const FkdGameplayTags& StateTags = FkdGameplayTags::Get();

        // Set final crush mode tag
        if (bNewCrushMode)
        {
            ASC->AddLooseGameplayTag(StateTags.State_CrushMode);
        }
        else
        {
            ASC->RemoveLooseGameplayTag(StateTags.State_CrushMode);
            // Remove all drain effects when exiting crush mode
            //if (ShadowDrainEffectHandle.IsValid())
            //{
            //    ASC->RemoveActiveGameplayEffect(ShadowDrainEffectHandle);
            //    ShadowDrainEffectHandle.Invalidate();
            //}
        }
        
		// Update shadow tracking in player
        AkdMyPlayer* Player = Cast<AkdMyPlayer>(CurrentActorInfo->AvatarActor.Get());
        if (Player && Player->CrushStateComponent)
        {
            Player->CrushStateComponent->ToggleShadowTracking(bNewCrushMode);
        }
        
        // Remove transitioning tag – but only if we are still the active ability
        if (ASC->HasMatchingGameplayTag(StateTags.State_Transitioning))
        {
            ASC->RemoveLooseGameplayTag(StateTags.State_Transitioning);
        }
    }

    // Unbind delegate to avoid multiple calls
    if (CachedTransitionComp)
    {
        CachedTransitionComp->OnTransitionComplete.RemoveDynamic(this, &Ukd_CrushToggle::OnTransitionFinished);
    }

    // End ability
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

