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

    // Set the cost effect
    static ConstructorHelpers::FObjectFinder<UClass> CostEffectClass(TEXT("/Game/ThePerspView/AbilitySystem/Effects/kdShadowDrain")); // adjust path
    if (CostEffectClass.Succeeded()) CostGameplayEffectClass = CostEffectClass.Object;
    
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

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) // consumes cost if any
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // call into player to perform vertical movement (player handles input)
    AActor* Avatar = ActorInfo->AvatarActor.Get();
    AkdMyPlayer* Player = Cast<AkdMyPlayer>(Avatar);

    if (Player)
    {
        // Launch the player upwards.
        Player->LaunchCharacter(FVector(0.f, 0.f, LaunchZStrength), true, true);

        // optionally add a gameplay cue for visual feedback here
    }

    // end the ability immediately
	EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}

void UkdShadowMove::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
    
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(FkdGameplayTags::Get().Ability_ShadowJump);
    }
}
