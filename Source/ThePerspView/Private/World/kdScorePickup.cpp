// Copyright ASKD Games

#include "World/kdScorePickup.h"
#include "GameMode/kdGameModeBase.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystem/kdAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"


AkdScorePickup::AkdScorePickup()
{
	PrimaryActorTick.bCanEverTick = false;

    PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
    SetRootComponent(PickupMesh);
    PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
    PickupSphere->SetupAttachment(RootComponent);
    PickupSphere->SetSphereRadius(60.f);
    PickupSphere->SetCollisionProfileName(TEXT("OverlapAll"));
    PickupSphere->SetGenerateOverlapEvents(true);

    // Gentle spin so the crystal catches the eye
    RotatingMovement = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingMovement"));
    RotatingMovement->RotationRate = FRotator(0.f, 90.f, 0.f);
}

void AkdScorePickup::BeginPlay()
{
	Super::BeginPlay();
	
    PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &AkdScorePickup::OnPickupSphereOverlap);
}

void AkdScorePickup::OnPickupSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (bCollected) return;

    AkdMyPlayer* Player = Cast<AkdMyPlayer>(OtherActor);
    if (!Player) return;

    // Only collectible while in Crush Mode — ensures it rewards shadow play
    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    if (!ASC) return;

    if (!ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode)) return;

    bCollected = true;

    // ── Award score ────────────────────────────────────────────────────────
    if (AkdGameModeBase* GM = Cast<AkdGameModeBase>(GetWorld()->GetAuthGameMode()))
    {
        GM->AddScore(ScoreValue);
    }

    // ── Restore stamina ────────────────────────────────────────────────────
    if (StaminaRestore > 0.f)
    {
        const float Current = ASC->GetNumericAttribute(UkdAttributeSet::GetShadowStaminaAttribute());
        const float Max = ASC->GetNumericAttribute(UkdAttributeSet::GetMaxShadowStaminaAttribute());
        const float NewVal = FMath::Clamp(Current + StaminaRestore, 0.f, Max);
        Cast<UkdAttributeSet>(
            const_cast<UAttributeSet*>(
                ASC->GetAttributeSet(UkdAttributeSet::StaticClass())
                )
        )->SetShadowStamina(NewVal);
    }

    // ── Feedback ───────────────────────────────────────────────────────────
    if (PickupSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
    }

    BP_OnCollected();

    // Hide mesh and disable collision — stay alive for sound to finish
    PickupMesh->SetVisibility(false);
    PickupSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Destroy after a short delay so any looping sound / particle finishes
    SetLifeSpan(1.5f);

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("ScorePickup [%s]: Collected. Score +%d  Stamina +%.0f"),
        *GetName(), ScoreValue, StaminaRestore);
#endif
}



