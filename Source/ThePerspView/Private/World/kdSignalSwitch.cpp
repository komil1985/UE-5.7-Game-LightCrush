// Copyright ASKD Games


#include "World/kdSignalSwitch.h"
#include "Interfaces/kdActivatable.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"


AkdSignalSwitch::AkdSignalSwitch()
{
    PrimaryActorTick.bCanEverTick = false;   // a relay has no per-frame work

    SwitchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwitchMesh"));
    SetRootComponent(SwitchMesh);

    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(200.f);
    // Same overlap-only profile the blocker used, so the player's ECC_Pawn
    // interact query returns this switch.
    InteractionSphere->SetCollisionProfileName(TEXT("OverlapAll"));
    InteractionSphere->SetGenerateOverlapEvents(true);
}

void AkdSignalSwitch::Interact(AkdMyPlayer* InInstigator)
{
    if (!InInstigator) return;

    if (!PassesModeGate(InInstigator))
    {
#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log, TEXT("SignalSwitch '%s': rejected — wrong world for ActivationMode"),
            *GetName());
#endif
        return;
    }

    int32 Delivered = 0;
    for (AActor* Target : Targets)
    {
        if (IkdActivatable* Device = Cast<IkdActivatable>(Target))
        {
            Device->Activate(this);
            ++Delivered;
        }
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("SignalSwitch '%s': pulsed %d/%d target(s)"),
        *GetName(), Delivered, Targets.Num());
#endif
}

bool AkdSignalSwitch::PassesModeGate(AkdMyPlayer* Player) const
{
    if (ActivationMode == EkdActivationMode::Both) return true;

    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    const bool bInCrush = ASC && ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode);

    return (ActivationMode == EkdActivationMode::CrushOnly) ? bInCrush : !bInCrush;
}
