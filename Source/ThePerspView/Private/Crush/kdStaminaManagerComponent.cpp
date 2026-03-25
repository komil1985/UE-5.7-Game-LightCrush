// Copyright ASKD Games


#include "Crush/kdStaminaManagerComponent.h"
#include "GameplayTags/kdGameplayTags.h"



UkdStaminaManagerComponent::UkdStaminaManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // check movement cooldown every 0.1s
}


void UkdStaminaManagerComponent::BeginPlay()
{
	Super::BeginPlay();

    ASC = GetOwner()->FindComponentByClass<UAbilitySystemComponent>();
    if (!ASC)
    {
#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Error, TEXT("StaminaManagerComponent requires an AbilitySystemComponent on the owner!"));
#endif
    }

    TimeSinceLastMove = RegenDelay; // start ready to regen if needed
    bWantsRegen = false;
    bIsInShadow = false;
    bIsMoving = false;
}


void UkdStaminaManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!ASC) return;

    // Update movement cooldown timer
    if (bIsMoving)
    {
        TimeSinceLastMove = 0.0f;
    }
    else
    {
        TimeSinceLastMove += DeltaTime;
    }

    // Determine whether we want regen effect
    bool bShouldRegen = (!bIsMoving && TimeSinceLastMove >= RegenDelay);
    if (bShouldRegen != bWantsRegen)
    {
        bWantsRegen = bShouldRegen;
        UpdateEffects();
    }

}

void UkdStaminaManagerComponent::SetInShadow(bool bInShadow)
{
    if (this->bIsInShadow == bInShadow) return;
    this->bIsInShadow = bInShadow;
    UpdateEffects();
}

void UkdStaminaManagerComponent::SetMoving(bool bMoving)
{
    if (this->bIsMoving == bMoving) return;
    this->bIsMoving = bMoving;
    // Cooldown timer will be reset in Tick, no need to update effects immediately
}

void UkdStaminaManagerComponent::UpdateEffects()
{
    if (!ASC) return;

    const FkdGameplayTags& Tags = FkdGameplayTags::Get();

    // Drain: requires in crush mode (handled by effect's tag requirements) + in shadow + moving
    bool bShouldDrain = bIsInShadow && bIsMoving; // Crush mode tag is checked by the effect's OngoingTagRequirements
    if (bShouldDrain && !DrainEffectHandle.IsValid() && DrainEffectClass)
    {
        FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
        Context.AddSourceObject(GetOwner());
        FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(DrainEffectClass, 1, Context);
        if (Spec.IsValid())
        {
            DrainEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
        }
    }
    else if (!bShouldDrain && DrainEffectHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(DrainEffectHandle);
        DrainEffectHandle.Invalidate();
    }

    // Regen: requires crush mode + not moving + after delay + stamina < max (effect should handle its own conditions via tags/periodic checks)
    bool bShouldRegen = bWantsRegen;
    if (bShouldRegen && !RegenEffectHandle.IsValid() && RegenEffectClass)
    {
        FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
        Context.AddSourceObject(GetOwner());
        FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(RegenEffectClass, 1, Context);
        if (Spec.IsValid())
        {
            RegenEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
        }
    }
    else if (!bShouldRegen && RegenEffectHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(RegenEffectHandle);
        RegenEffectHandle.Invalidate();
    }
}

