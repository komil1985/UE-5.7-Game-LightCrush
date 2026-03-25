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

	AbilityTags.AddTag(StateTags.Ability_ShadowJump);

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

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) // consumes cost if any
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // call into player to perform vertical movement (player handles input)
    AActor* Avatar = ActorInfo->AvatarActor.Get();
    AkdMyPlayer* Player = Cast<AkdMyPlayer>(Avatar);

    const FkdGameplayTags& Tags = FkdGameplayTags::Get();

    if (Player)
    {
        // Launch the player upwards.
        Player->LaunchCharacter(FVector(0.f, 0.f, LaunchZStrength), false, true);
#if !UE_BUILD_SHIPPING
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Jump in shadow!"));
		UE_LOG(LogTemp, Log, TEXT("Player launched with strength: %f"), LaunchZStrength);
#endif
        // optionally add a gameplay cue for visual feedback here
    }
    else
    {
#if !UE_BUILD_SHIPPING
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Can't jump in shadow!"));
		UE_LOG(LogTemp, Warning, TEXT("Player does not have the Ability_ShadowJump tag, cannot perform shadow jump."));
#endif
    }

    // end the ability immediately
	EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}

void UkdShadowMove::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
