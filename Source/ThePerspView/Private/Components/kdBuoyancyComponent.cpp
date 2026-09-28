// Copyright ASKD Games

#include "Components/kdBuoyancyComponent.h"
#include "Subsystem/kdBuoyancyRippleSubsystem.h"
#include "Components/kdGeometryTransitionComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Crush/kdCrushDirectionLibrary.h"
#include "Player/kdMyPlayer.h"
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

    // Deliberately UNATTACHED and positioned purely in world space — zero
    // dependency on the owner's RootComponent (see AkdFloorBase history).
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

        WeightTrigger->OnComponentBeginOverlap.AddDynamic(this, &UkdBuoyancyComponent::OnWeightTriggerBeginOverlap);
        WeightTrigger->OnComponentEndOverlap.AddDynamic(this, &UkdBuoyancyComponent::OnWeightTriggerEndOverlap);

        FitWeightTriggerToRestPose();

        // The sibling geometry component may snap the mesh into its crush pose
        // in ITS BeginPlay (level starting already in Crush Mode), which can
        // run after ours — so refit once more on the first tick.
        bTriggerDirty = true;
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

// ─────────────────────────────────────────────────────────────────────────────
// Weight sensor fitting
// ─────────────────────────────────────────────────────────────────────────────

void UkdBuoyancyComponent::UpdateWeightTriggerFit()
{
    if (!WeightTrigger) return;

    // While the geometry component is shivering / sliding the mesh its bounds
    // are in flux — wait for it to settle, then refit exactly once.
    if (SiblingGeometryTransition && SiblingGeometryTransition->IsTransitioning())
    {
        bTriggerDirty = true;
        return;
    }

    if (bTriggerDirty)
    {
        bTriggerDirty = false;
        FitWeightTriggerToRestPose();
    }
}

void UkdBuoyancyComponent::FitWeightTriggerToRestPose()
{
    if (!WeightTrigger || !CachedMesh) return;

    const bool bCrushed = SiblingGeometryTransition && SiblingGeometryTransition->IsCrushed();

    // Current world AABB with the live sink offset removed = the REST pose.
    // Reading the live bounds (not cached 3D numbers) is what lets the sensor
    // follow the mesh when it collapses onto the crush plane.
    const FBoxSphereBounds B = CachedMesh->Bounds;
    FVector Center = B.Origin;
    Center.Z -= DisplacementZ;
    FVector Extent = B.BoxExtent;
    const float MeshHalfZ = Extent.Z;

    if (bCrushed)
    {
        // The slab is ~1% as thick on the collapse axis. The capsule is
        // plane-constrained and centred on that plane, so thicken the sensor
        // there to guarantee overlap. Axis via the basis — never hardcoded.
        const AkdMyPlayer* Player = Cast<AkdMyPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
        const bool bCollapsesY = Player && UkdCrushDirectionLibrary::MakeCrushBasis(Player->GetActiveCrushDirection()).bCollapsesY;

        float CollapseHalf = bCollapsesY ? Extent.Y : Extent.X;
        CollapseHalf = FMath::Max(CollapseHalf, CrushPlaneMinHalfThickness);
    }
    else
    {
        if (!ManualTriggerExtent.IsNearlyZero())
        {
            Extent.X = ManualTriggerExtent.X;
            Extent.Y = ManualTriggerExtent.Y;
        }
        Center += TriggerLocationOffset;
    }

    const float TopZ = Center.Z + MeshHalfZ;
    const float BoxBottomZ = TopZ - SinkDepth - 5.f;   // covers the full sink range
    const float BoxTopZ = TopZ + AutoFitTopMargin;
    const float BoxHalfHeight = FMath::Max(1.f, (BoxTopZ - BoxBottomZ) * 0.5f);

    WeightTrigger->SetBoxExtent(FVector(Extent.X, Extent.Y, BoxHalfHeight));
    WeightTrigger->SetWorldLocation(FVector(Center.X, Center.Y, (BoxTopZ + BoxBottomZ) * 0.5f));

    // Explicit refresh so a player already standing inside the new box is
    // detected immediately instead of waiting for his next movement.
    WeightTrigger->UpdateOverlaps();

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log, TEXT("Buoyancy [%s]: sensor fit (%s) extent=%s centre=%s"),
        *GetNameSafe(GetOwner()), bCrushed ? TEXT("2D") : TEXT("3D"),
        *WeightTrigger->GetScaledBoxExtent().ToString(),
        *WeightTrigger->GetComponentLocation().ToString());
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Overlap
// ─────────────────────────────────────────────────────────────────────────────

void UkdBuoyancyComponent::OnWeightTriggerBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
    UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
    // Only track one player at a time — same convention as AkdShadowEnemy's DamageSphere.
    if (OverlappingPlayer.IsValid()) return;

    AkdMyPlayer* Player = Cast<AkdMyPlayer>(OtherActor);
    if (!Player) return;

    OverlappingPlayer = Player;
    bIsLoaded = true;

    // Landing impulse: fold the player's downward speed into the spring.
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

    // Ripple a falloff-scaled fraction of THIS impact to nearby platforms.
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

// ─────────────────────────────────────────────────────────────────────────────
// Ripple API
// ─────────────────────────────────────────────────────────────────────────────

FVector UkdBuoyancyComponent::GetMeshWorldLocation() const
{
    return CachedMesh ? CachedMesh->GetComponentLocation() : FVector::ZeroVector;
}

void UkdBuoyancyComponent::ReceiveRippleImpulse(float VelocityKick)
{
    VelocityZ -= VelocityKick;
    BP_OnRippleReceived(VelocityKick);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick
// ─────────────────────────────────────────────────────────────────────────────

void UkdBuoyancyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedMesh) return;

    UpdateWeightTriggerFit();

    const float Target = bIsLoaded ? -SinkDepth : 0.f;

    // Fully at rest and unloaded: skip the spring AND the mesh write. Most
    // platforms spend nearly all their life here, so this keeps the idle cost
    // to a few float compares (and avoids re-dirtying overlaps every frame).
    if (FMath::IsNearlyZero(Target)
        && FMath::IsNearlyZero(DisplacementZ, 0.02f)
        && FMath::IsNearlyZero(VelocityZ, 0.1f))
    {
        if (DisplacementZ != 0.f || VelocityZ != 0.f)
        {
            DisplacementZ = 0.f;
            VelocityZ = 0.f;
            ApplyDisplacement();   // final exact-rest write
        }
        return;
    }

    StepSpring(DisplacementZ, VelocityZ, Target, DampingRatio, Frequency, DeltaTime);
    ApplyDisplacement();
}

void UkdBuoyancyComponent::ApplyDisplacement()
{
    if (SiblingGeometryTransition)
    {
        // The geometry component is the SOLE writer of this mesh's transform.
        // We only hand it the number; it layers it onto every shiver / morph /
        // rest write, so the sink survives a crush transition with no pop.
        SiblingGeometryTransition->SetExternalZOffset(DisplacementZ);
        return;
    }

    FVector NewLoc = OriginalMeshRelativeLoc;
    NewLoc.Z += DisplacementZ;
    CachedMesh->SetRelativeLocation(NewLoc);
}

// ─────────────────────────────────────────────────────────────────────────────
// StepSpring — implicit (backward-Euler) damped harmonic oscillator.
//
// Continuous model:  x'' = -w^2*(x - target) - 2*zeta*w*x'
// Solved implicitly per-tick so the result is unconditionally stable for any
// DeltaTime / Frequency / DampingRatio.
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