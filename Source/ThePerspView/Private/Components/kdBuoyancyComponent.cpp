// Copyright ASKD Games

#include "Components/kdBuoyancyComponent.h"
#include "Subsystem/kdBuoyancyRippleSubsystem.h"
#include "Components/kdGeometryTransitionComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Audio/kdAudioSubsystem.h"
#include "Kismet/GameplayStatics.h"

UkdBuoyancyComponent::UkdBuoyancyComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UkdBuoyancyComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        UE_LOG(LogTemp, Error, TEXT("Buoyancy: component has no owner — disabled."));
        return;
    }

    CachedMesh = Owner->FindComponentByClass<UStaticMeshComponent>();
    if (!CachedMesh)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Buoyancy [%s]: No UStaticMeshComponent found on owner — disabled."),
            *Owner->GetName());
        return;
    }

    if (CachedMesh->Mobility != EComponentMobility::Movable)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Buoyancy [%s]: '%s' is not Movable mobility — it will NOT animate at runtime. "
                "Set the mesh's Mobility to Movable."),
            *Owner->GetName(), *CachedMesh->GetName());
    }

    OriginalMeshRelativeLoc = CachedMesh->GetRelativeLocation();
    SiblingGeometryTransition = Owner->FindComponentByClass<UkdGeometryTransitionComponent>();

    // Deliberately UNATTACHED (no parent) and positioned purely in world
    // space. This has zero dependency on the owning actor's RootComponent —
    // an earlier attached-to-root version silently created no sensor at all
    // on any actor whose RootComponent was null, which permanently disabled
    // the whole effect with no error.
    WeightTrigger = NewObject<UBoxComponent>(Owner, UBoxComponent::StaticClass(), TEXT("BuoyancyWeightTrigger"));
    if (WeightTrigger)
    {
        WeightTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        WeightTrigger->SetCollisionObjectType(ECC_WorldDynamic);
        WeightTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
        WeightTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
        WeightTrigger->SetGenerateOverlapEvents(true);
        WeightTrigger->SetHiddenInGame(true);
        WeightTrigger->ShapeColor = FColor(80, 160, 220);   // pale ion — editor legibility only
        WeightTrigger->RegisterComponent();

        AutoFitWeightTrigger();

        WeightTrigger->OnComponentBeginOverlap.AddDynamic(this, &UkdBuoyancyComponent::OnWeightTriggerBeginOverlap);
        WeightTrigger->OnComponentEndOverlap.AddDynamic(this, &UkdBuoyancyComponent::OnWeightTriggerEndOverlap);

        UE_LOG(LogTemp, Log,
            TEXT("Buoyancy [%s]: sensor ready — mesh='%s' movable=%d  extent=%s  worldLoc=%s"),
            *Owner->GetName(), *CachedMesh->GetName(),
            CachedMesh->Mobility == EComponentMobility::Movable,
            *WeightTrigger->GetScaledBoxExtent().ToString(),
            *WeightTrigger->GetComponentLocation().ToString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Buoyancy [%s]: failed to create weight sensor."), *Owner->GetName());
    }

    if (UWorld* World = GetWorld())
    {
        if (UkdBuoyancyRippleSubsystem* Ripple = World->GetSubsystem<UkdBuoyancyRippleSubsystem>())
        {
            Ripple->RegisterPlatform(this);
        }
    }
}

void UkdBuoyancyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (UkdBuoyancyRippleSubsystem* Ripple = World->GetSubsystem<UkdBuoyancyRippleSubsystem>())
        {
            Ripple->UnregisterPlatform(this);
        }
    }

    if (WeightTrigger)
    {
        WeightTrigger->OnComponentBeginOverlap.RemoveDynamic(this, &UkdBuoyancyComponent::OnWeightTriggerBeginOverlap);
        WeightTrigger->OnComponentEndOverlap.RemoveDynamic(this, &UkdBuoyancyComponent::OnWeightTriggerEndOverlap);
        WeightTrigger->DestroyComponent();
        WeightTrigger = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

void UkdBuoyancyComponent::AutoFitWeightTrigger()
{
    if (!WeightTrigger || !CachedMesh) return;

    // World-space AABB, captured BEFORE any sink offset has ever been
    // applied — this is genuinely the platform's rest pose. Using world
    // Bounds (not local mesh-space math) means this works regardless of the
    // mesh's own relative rotation/scale, or the owning actor's hierarchy.
    const FBoxSphereBounds WorldBounds = CachedMesh->Bounds;

    const FVector Extent = !ManualTriggerExtent.IsNearlyZero()
        ? ManualTriggerExtent
        : WorldBounds.BoxExtent;

    const float TopZ = WorldBounds.Origin.Z + WorldBounds.BoxExtent.Z;
    const float BoxBottomZ = TopZ - SinkDepth - 5.f;   // covers the full sink range
    const float BoxTopZ = TopZ + AutoFitTopMargin;
    const float BoxHalfHeight = FMath::Max(1.f, (BoxTopZ - BoxBottomZ) * 0.5f);
    const float BoxCenterZ = (BoxTopZ + BoxBottomZ) * 0.5f;

    WeightTrigger->SetBoxExtent(FVector(Extent.X, Extent.Y, BoxHalfHeight));
    WeightTrigger->SetWorldLocation(
        FVector(WorldBounds.Origin.X, WorldBounds.Origin.Y, BoxCenterZ) + TriggerLocationOffset);
}

void UkdBuoyancyComponent::OnWeightTriggerBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
    UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
    // Only track one player at a time — same convention as AkdShadowEnemy's DamageSphere.
    if (OverlappingPlayer.IsValid()) return;

    AkdMyPlayer* Player = Cast<AkdMyPlayer>(OtherActor);
    if (!Player) return;

    OverlappingPlayer = Player;
    bIsLoaded = true;

    // Landing impulse: fold the player's current downward speed into the
    // spring as an instant velocity kick, so a hard landing dips deeper than
    // a gentle step, then settles back to the same steady equilibrium.
    float LocalKick = 0.f;
    if (ImpulseToVelocityScale > KINDA_SMALL_NUMBER)
    {
        if (const UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement())
        {
            const float DownwardSpeed = FMath::Max(0.f, -MoveComp->Velocity.Z);
            const float Clamped = FMath::Min(DownwardSpeed, MaxLandingSpeedForImpulse);
            LocalKick = Clamped * ImpulseToVelocityScale;
            VelocityZ -= LocalKick;
        }
    }

    // Ripple: pass a falloff-scaled fraction of THIS impact out to nearby
    // buoyancy platforms so a connected raft shudders together.
    if (LocalKick > KINDA_SMALL_NUMBER && RippleRadius > KINDA_SMALL_NUMBER)
    {
        if (UWorld* World = GetWorld())
        {
            if (UkdBuoyancyRippleSubsystem* Ripple = World->GetSubsystem<UkdBuoyancyRippleSubsystem>())
            {
                Ripple->BroadcastImpulse(this, GetMeshWorldLocation(), LocalKick, RippleRadius, RippleStrength);
            }
        }
    }

    if (LoadSound)
    {
        if (UkdAudioSubsystem* Audio = UkdAudioSubsystem::Get(this))
        {
            Audio->PlaySFX2D(LoadSound);
        }
    }

    BP_OnPlatformLoaded();
}

void UkdBuoyancyComponent::OnWeightTriggerEndOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
    UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
    if (OverlappingPlayer.Get() != OtherActor) return;

    OverlappingPlayer = nullptr;
    bIsLoaded = false;

    if (UnloadSound)
    {
        if (UkdAudioSubsystem* Audio = UkdAudioSubsystem::Get(this))
        {
            Audio->PlaySFX2D(UnloadSound);
        }
    }

    BP_OnPlatformUnloaded();
}

bool UkdBuoyancyComponent::IsSuspendedForCrushHandoff() const
{
    if (!SiblingGeometryTransition) return false;

    const AActor* Owner = GetOwner();
    if (!Owner) return false;

    const AkdMyPlayer* Player = Cast<AkdMyPlayer>(UGameplayStatics::GetPlayerPawn(Owner, 0));
    const UAbilitySystemComponent* ASC = Player ? Player->GetAbilitySystemComponent() : nullptr;
    if (!ASC) return false;

    const FkdGameplayTags& Tags = FkdGameplayTags::Get();
    return ASC->HasMatchingGameplayTag(Tags.State_CrushMode)
        || ASC->HasMatchingGameplayTag(Tags.State_Transitioning);
}

FVector UkdBuoyancyComponent::GetMeshWorldLocation() const
{
    return CachedMesh ? CachedMesh->GetComponentLocation() : FVector::ZeroVector;
}

void UkdBuoyancyComponent::ReceiveRippleImpulse(float VelocityKick)
{
    if (IsSuspendedForCrushHandoff()) return;   // don't queue velocity for a suspended spring

    VelocityZ -= VelocityKick;
    BP_OnRippleReceived(VelocityKick);
}

void UkdBuoyancyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedMesh) return;

    if (IsSuspendedForCrushHandoff())
    {
        // Hand transform authority to UkdGeometryTransitionComponent with no
        // pop: snap our own state to neutral once, then stop writing entirely.
        if (!bWasSuspended)
        {
            DisplacementZ = 0.f;
            VelocityZ = 0.f;
            bWasSuspended = true;
        }
        return;
    }

    if (bWasSuspended)
    {
        bWasSuspended = false;
        DisplacementZ = 0.f;
        VelocityZ = 0.f;
    }

    const float Target = bIsLoaded ? -SinkDepth : 0.f;
    StepSpring(DisplacementZ, VelocityZ, Target, DampingRatio, Frequency, DeltaTime);

    FVector NewLoc = OriginalMeshRelativeLoc;
    NewLoc.Z += DisplacementZ;
    CachedMesh->SetRelativeLocation(NewLoc);
}

// ─────────────────────────────────────────────────────────────────────────────
// StepSpring — implicit (backward-Euler) damped harmonic oscillator.
//
// Continuous model:  x'' = -w^2*(x - target) - 2*zeta*w*x'
// Solved implicitly per-tick so the result is unconditionally stable for any
// DeltaTime / Frequency / DampingRatio — it can never diverge on a frame
// hitch, unlike an explicit ("x += ... * dt") spring.
// ─────────────────────────────────────────────────────────────────────────────
void UkdBuoyancyComponent::StepSpring(float& X, float& V, float InTarget, float InDampingRatio, float InFrequencyHz, float InDeltaTime)
{
    if (InDeltaTime <= 0.f) return;

    const float Omega = 2.f * PI * FMath::Max(InFrequencyHz, 0.05f);
    const float Zeta = FMath::Max(InDampingRatio, 0.f);

    const float F = 1.f + 2.f * InDeltaTime * Zeta * Omega;
    const float OmegaSq = Omega * Omega;
    const float H_OmegaSq = InDeltaTime * OmegaSq;
    const float HH_OmegaSq = InDeltaTime * H_OmegaSq;
    const float DetInv = 1.f / (F + HH_OmegaSq);

    const float NewX = (F * X + InDeltaTime * V + HH_OmegaSq * InTarget) * DetInv;
    const float NewV = (V + H_OmegaSq * (InTarget - X)) * DetInv;

    X = NewX;
    V = NewV;
}