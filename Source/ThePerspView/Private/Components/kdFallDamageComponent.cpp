// Copyright ASKD Games


#include "Components/kdFallDamageComponent.h"
#include "Player/kdMyPlayer.h"
#include "GameMode/kdGameModeBase.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"


UkdFallDamageComponent::UkdFallDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


void UkdFallDamageComponent::BeginPlay()
{
	Super::BeginPlay();

    CachedPlayer = Cast<AkdMyPlayer>(GetOwner());
    if (!CachedPlayer)
    {
        UE_LOG(LogTemp, Error,
            TEXT("UkdFallDamageComponent: Owner is not AkdMyPlayer."));
        return;
    }

    // ACharacter::LandedDelegate fires every time the character touches the
    // ground after being airborne — binding here is cleaner than overriding
    // Landed() in the player class.
    CachedPlayer->LandedDelegate.AddDynamic(this, &UkdFallDamageComponent::OnPlayerLanded);
}

void UkdFallDamageComponent::OnPlayerLanded(const FHitResult& Hit)
{
    if (!CachedPlayer) return;

    // ── Crush Mode guard ──────────────────────────────────────────────────────
    // In Crush Mode the player glides on the shadow plane with GravityScale=0,
    // so real fall impacts don't happen. If somehow Landed fires anyway (e.g.
    // transitioning while airborne), skip the check entirely.
    if (bIgnoreFallsInCrushMode)
    {
        UAbilitySystemComponent* ASC = CachedPlayer->GetAbilitySystemComponent();
        if (ASC && ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode))
        {
            return;
        }
    }

    // ── Measure impact speed ──────────────────────────────────────────────────
    // GetMovementComponent()->Velocity at the moment Landed fires still holds
    // the pre-landing velocity, which is what we want to measure.
    UCharacterMovementComponent* MoveComp = CachedPlayer->GetCharacterMovement();
    if (!MoveComp) return;

    // Z is negative when falling; take absolute value
    const float ImpactSpeed = FMath::Abs(MoveComp->Velocity.Z);

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("FallDamage: Landing at %.0f cm/s (Fatal=%.0f Hard=%.0f)"),
        ImpactSpeed, FatalFallSpeed, HardLandSpeed);
#endif

    // ── Fatal fall ────────────────────────────────────────────────────────────
    if (ImpactSpeed >= FatalFallSpeed)
    {
        if (FatalLandSound)
        {
            UGameplayStatics::PlaySoundAtLocation(
                this, FatalLandSound, CachedPlayer->GetActorLocation());
        }

        if (AkdGameModeBase* GM =
            Cast<AkdGameModeBase>(GetWorld()->GetAuthGameMode()))
        {
            GM->HandlePlayerDeath();
        }
        return;  // don't also play hard-land feedback if fatal
    }

    // ── Hard land (visual feedback only, no death) ────────────────────────────
    if (ImpactSpeed >= HardLandSpeed)
    {
        if (HardLandSound)
        {
            UGameplayStatics::PlaySoundAtLocation(
                this, HardLandSound, CachedPlayer->GetActorLocation());
        }
        BP_OnHardLand(ImpactSpeed);
    }
}

