// Copyright ASKD Games


#include "AbilitySystem/Abilities/kd_CrushToggle.h"
#include "GameplayTags/kdGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Player/kdMyPlayer.h"
#include "Crush/kdCrushTransitionComponent.h"
#include "Crush/kdCrushStateComponent.h"
#include "Components/kdGameFeedbackComponent.h"



Ukd_CrushToggle::Ukd_CrushToggle()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // Add the ability tag so it can be found by tag activation
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

    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!ASC) return;

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
    AkdMyPlayer* Player = Cast <AkdMyPlayer>(AvatarActor);
    if (!Player)
    {
        UE_LOG(LogTemp, Warning, TEXT("CrushToggle: AvatarActor is not AkdMyPlayer — aborting."));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }
    CachedTransitionComp = Player->CrushTransitionComponent;
    if (!CachedTransitionComp)
    {
        UE_LOG(LogTemp, Warning, TEXT("CrushToggle: No CrushTransitionComponent — aborting."));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    const FkdGameplayTags& Tags = FkdGameplayTags::Get();

    const bool bTargetCrushMode = !ASC->HasMatchingGameplayTag(Tags.State_CrushMode);

    // ── 4. IMMEDIATE feedback — fires before anticipation delay ───────────────
    //
    // OnCrushTransitionStarted runs right now so the player gets instant
    // audio-visual confirmation that input was registered, even before the
    // scale punch and morph begin.  This is the most important frame for feel.
    if (UkdGameFeedbackComponent* GFC = Player->FindComponentByClass<UkdGameFeedbackComponent>())
    {
        GFC->OnCrushTransitionStarted(bTargetCrushMode);
    }


    // --- Apply drain effect when entering crush mode ---
    if (bTargetCrushMode && CrushDrainEffect)
    {
        FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CrushDrainEffect, GetAbilityLevel());
        if (SpecHandle.IsValid())
        {
            ShadowDrainEffectHandle = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
        }
    }

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
        CachedTransitionComp = nullptr;
    }

    // Optionally remove drain effect if still active
    if (ShadowDrainEffectHandle.IsValid())
    {
        if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
        {
            ASC->RemoveActiveGameplayEffect(ShadowDrainEffectHandle);
        }
        ShadowDrainEffectHandle.Invalidate();
    }

    // ── Remove drain effect if the ability was cancelled mid-transition ────────
    RemoveDrainEffect();

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void Ukd_CrushToggle::OnTransitionFinished(bool bNewCrushMode)
{
	UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
    if (!ASC)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("CrushToggle::OnTransitionFinished — ASC is null, ending ability."));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    // ── Landing feedback — fires at the first frame the morph is complete ──────
    //
    // Called BEFORE tag changes so OnCrushModeTagChanged in GFC sees the correct
    // bInCrushMode for the state we are LEAVING, not the one we are entering.
    // (The landing echo is about the physical "thud" of arrival, not the new state.)
    AkdMyPlayer* Player = Cast<AkdMyPlayer>(CurrentActorInfo->AvatarActor.Get());
    if (Player)
    {
        if (UkdGameFeedbackComponent* GFC =
            Player->FindComponentByClass<UkdGameFeedbackComponent>())
        {
            GFC->OnCrushTransitionFinished(bNewCrushMode);
        }
    }

    const FkdGameplayTags& StateTags = FkdGameplayTags::Get();

    // Set final crush mode tag
    if (bNewCrushMode)
    {
        ASC->AddLooseGameplayTag(StateTags.State_CrushMode);
    }
    else
    {
        ASC->RemoveLooseGameplayTag(StateTags.State_CrushMode);

        // Clean up InShadow if lingering
        if (ASC->HasMatchingGameplayTag(StateTags.State_InShadow))
        {
            ASC->RemoveLooseGameplayTag(StateTags.State_InShadow);
        }

        RemoveDrainEffect();

        // Remove all drain effects when exiting crush mode
        FGameplayTagContainer DrainTags;
        DrainTags.AddTag(StateTags.Effect_ShadowDrain);
        ASC->RemoveActiveEffectsWithGrantedTags(DrainTags);
            
        // clear out stored handle if it was still active
        if (ShadowDrainEffectHandle.IsValid())
        {
            ASC->RemoveActiveGameplayEffect(ShadowDrainEffectHandle);
            ShadowDrainEffectHandle.Invalidate();
        }
    }

    // Update shadow tracking in player
    //AkdMyPlayer* Player = Cast<AkdMyPlayer>(CurrentActorInfo->AvatarActor.Get());
    if (Player && Player->CrushStateComponent)
    {
        Player->CrushStateComponent->ToggleShadowTracking(bNewCrushMode);
    }
        
    // Remove transitioning tag – but only if we are still the active ability
    //if (ASC->HasMatchingGameplayTag(StateTags.State_Transitioning))
    //{
    //    ASC->RemoveLooseGameplayTag(StateTags.State_Transitioning);
    //}
    
    // Unbind delegate to avoid multiple calls
    if (CachedTransitionComp)
    {
        CachedTransitionComp->OnTransitionComplete.RemoveDynamic(this, &Ukd_CrushToggle::OnTransitionFinished);
    }

    // End ability
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void Ukd_CrushToggle::RemoveDrainEffect()
{
    if (!ShadowDrainEffectHandle.IsValid()) return;

    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->RemoveActiveGameplayEffect(ShadowDrainEffectHandle);
    }

    ShadowDrainEffectHandle.Invalidate();
}

