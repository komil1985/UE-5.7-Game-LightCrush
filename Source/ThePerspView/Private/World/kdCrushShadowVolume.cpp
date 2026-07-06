// Copyright ASKD Games

#include "World/kdCrushShadowVolume.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Player/kdMyPlayer.h"
#include "Crush/kdCrushDirectionLibrary.h"
#include "Components/kdWorldColorDriver.h"

// Material parameter names — must match the M_CrushShadowVolume graph.
const FName AkdCrushShadowVolume::MP_ShadowColor = TEXT("ShadowColor");
const FName AkdCrushShadowVolume::MP_ShadowOpacity = TEXT("ShadowOpacity");


AkdCrushShadowVolume::AkdCrushShadowVolume()
{
    // Tick is enabled per-transition and disabled at rest — zero idle cost.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    VolumeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VolumeMesh"));
    SetRootComponent(VolumeMesh);

    // Engine unit cube: 100 cm per side, centred pivot.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeFinder.Succeeded())
    {
        VolumeMesh->SetStaticMesh(CubeFinder.Object);
    }

    // Baseline = Custom-Depth-ONLY: never drawn in colour, casts nothing, collides
    // with nothing. If a tint material is assigned, BeginPlay re-enables the main
    // pass so the translucent slab can render; otherwise this legacy state stands.
    VolumeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VolumeMesh->SetCanEverAffectNavigation(false);
    VolumeMesh->SetCastShadow(false);
    VolumeMesh->bRenderInMainPass = false;                 // invisible in the scene
    VolumeMesh->SetRenderCustomDepth(false);               // enabled only in Crush Mode
    VolumeMesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
}

void AkdCrushShadowVolume::ApplyZoneScale(bool bCollapsesY)
{
    // Thin depth on the collapse axis; wide on the in-plane horizontal axis.
    const FVector Scale = bCollapsesY
        ? FVector(ZoneWidthY / 100.f, ZoneDepthX / 100.f, ZoneHeightZ / 100.f)  // Y-crush: thin on Y
        : FVector(ZoneDepthX / 100.f, ZoneWidthY / 100.f, ZoneHeightZ / 100.f); // X-crush: thin on X
    VolumeMesh->SetWorldScale3D(Scale);
}

bool AkdCrushShadowVolume::PositionForCurrentCrush()
{
    if (!CachedPlayer)
    {
        return false;
    }

    const FkdCrushBasis Basis = UkdCrushDirectionLibrary::MakeCrushBasis(
        CachedPlayer->GetActiveCrushDirection());

    // Per-volume axis filter: a volume may opt to appear only on X-crush,
    // only on Y-crush, or on both.
    const bool bAxisAllowed =
        (ActivationAxisFilter == EkdCrushAxisFilter::AnyAxis) ||
        (ActivationAxisFilter == EkdCrushAxisFilter::YAxisOnly && Basis.bCollapsesY) ||
        (ActivationAxisFilter == EkdCrushAxisFilter::XAxisOnly && !Basis.bCollapsesY);

    if (!bAxisAllowed)
    {
        return false;   // this volume sits out the current crush direction
    }

    ApplyZoneScale(Basis.bCollapsesY);

    if (bTrackPlayerDepth)
    {
        const FVector PlayerLoc = CachedPlayer->GetActorLocation();
        const float   Behind = DepthBehindPlayer + (ZoneDepthX * 0.5f);
        FVector       Loc = OriginalPlacedLocation;

        if (Basis.bCollapsesY)
            Loc.Y = PlayerLoc.Y + Basis.ViewForward.Y * Behind;
        else
            Loc.X = PlayerLoc.X + Basis.ViewForward.X * Behind;

        SetActorLocation(Loc);
    }

    return true;
}

void AkdCrushShadowVolume::EnsureDynamicMaterial()
{
    // No source material assigned → keep the legacy stencil-only behaviour. The
    // mesh stays out of the main pass and nothing coloured ever draws.
    if (!ShadowVolumeMaterial)
    {
        return;
    }

    // One dynamic instance per volume so ShadowColor / ShadowOpacity are per-instance.
    ShadowMID = VolumeMesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, ShadowVolumeMaterial);

    if (ShadowMID)
    {
        // Promote the mesh into the colour pass so the translucent slab can render.
        // It stays at zero opacity / hidden until Crush Mode activates it.
        VolumeMesh->SetRenderInMainPass(true);
        RefreshMaterialParameters();
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("AkdCrushShadowVolume [%s]: Failed to create dynamic material from %s."),
            *GetName(), *GetNameSafe(ShadowVolumeMaterial));
    }
}

void AkdCrushShadowVolume::RefreshMaterialParameters()
{
    if (!ShadowMID)
    {
        return;
    }

    ShadowMID->SetVectorParameterValue(MP_ShadowColor, ShadowColor);
    ShadowMID->SetScalarParameterValue(MP_ShadowOpacity, ShadowOpacity);
}

void AkdCrushShadowVolume::SnapToState(bool bInCrush)
{
    // Non-animated set: used at level start and as the fallback when there is no
    // tint material or no colour driver to sync a fade against.
    bVisibleThisCrush = bInCrush && PositionForCurrentCrush();

    VolumeMesh->SetVisibility(bVisibleThisCrush);
    VolumeMesh->SetRenderCustomDepth(bVisibleThisCrush && bWriteShadowStencil);

    if (ShadowMID)
    {
        ShadowMID->SetVectorParameterValue(MP_ShadowColor, ShadowColor);
        ShadowMID->SetScalarParameterValue(MP_ShadowOpacity,
            bVisibleThisCrush ? ShadowOpacity : 0.f);
    }

    SetActorTickEnabled(false);
}

void AkdCrushShadowVolume::BeginPlay()
{
    Super::BeginPlay();

    OriginalPlacedLocation = GetActorLocation();

    ApplyZoneScale(false);

    // Stencil / shadow-suppression setup (unchanged legacy pipeline).
    VolumeMesh->SetCastShadow(false);                    // no shadow from the box
    VolumeMesh->bCastHiddenShadow = false;               // belt-and-suspenders for hidden-mesh shadows
    VolumeMesh->SetCustomDepthStencilValue(ShadowStencilValue);

    // Build the per-instance tint material (only if one is assigned). This may
    // flip the mesh into the main pass; visibility is still gated by Crush Mode.
    EnsureDynamicMaterial();

    CachedPlayer = Cast<AkdMyPlayer>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (!CachedPlayer)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("AkdCrushShadowVolume [%s]: No AkdMyPlayer at BeginPlay."), *GetName());
        return;
    }

    // Cache the colour driver (read-only) so the slab fade can ride its blend alpha.
    CachedColorDriver = CachedPlayer->FindComponentByClass<UkdWorldColorDriver>();

    CachedASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(CachedPlayer);
    if (!CachedASC)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("AkdCrushShadowVolume [%s]: Player has no ASC."), *GetName());
        return;
    }

    // Same registration pattern as AkdShadowPortal / UkdWorldColorDriver.
    CrushTagHandle = CachedASC->RegisterGameplayTagEvent(
        FkdGameplayTags::Get().State_CrushMode,
        EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &AkdCrushShadowVolume::OnCrushModeTagChanged);

    // Honour current state WITHOUT a fade — if we start already in Crush Mode the
    // slab should simply be present, not develop in on level load.
    SnapToState(CachedASC->GetTagCount(FkdGameplayTags::Get().State_CrushMode) > 0);
}

void AkdCrushShadowVolume::EndPlay(const EEndPlayReason::Type Reason)
{
    if (CachedASC && CrushTagHandle.IsValid())
    {
        CachedASC->RegisterGameplayTagEvent(
            FkdGameplayTags::Get().State_CrushMode,
            EGameplayTagEventType::NewOrRemoved).Remove(CrushTagHandle);
    }
    Super::EndPlay(Reason);
}

#if WITH_EDITOR
void AkdCrushShadowVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // Immediate feedback while tuning in a running PIE session. During an active
    // fade the tick overwrites opacity next frame; at rest this sets the endpoint.
    RefreshMaterialParameters();
}
#endif

void AkdCrushShadowVolume::OnCrushModeTagChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
    bTargetCrushActive = (NewCount > 0);

    // A driver-synced fade needs both a tint material AND the colour driver. Without
    // either we fall back to the original binary snap so nothing regresses.
    const bool bCanFade = (ShadowMID != nullptr) && (CachedColorDriver != nullptr);

    if (bTargetCrushActive)
    {
        // Entering: resolve axis + placement now so the fade develops in the right
        // spot. bVisibleThisCrush is latched here and reused on the matching exit.
        bVisibleThisCrush = PositionForCurrentCrush();

        if (!bVisibleThisCrush)
        {
            // This volume sits out the current crush direction — stay hidden, no fade.
            SnapToState(false);
            return;
        }

        // Show now; opacity begins at ~0 and the driver-synced tick ramps it in,
        // so there is no first-frame pop even if the driver hasn't ticked yet.
        VolumeMesh->SetVisibility(true);
        VolumeMesh->SetRenderCustomDepth(bWriteShadowStencil);
        if (ShadowMID)
        {
            ShadowMID->SetVectorParameterValue(MP_ShadowColor, ShadowColor);
            ShadowMID->SetScalarParameterValue(MP_ShadowOpacity, 0.f);
        }
    }
    else
    {
        // Exiting: if this volume was never shown this crush, there is nothing to
        // fade out — snap hidden and bail.
        if (!bVisibleThisCrush)
        {
            SnapToState(false);
            return;
        }
        // Otherwise keep the current placement and let the tick fade the slab out.
    }

    if (bCanFade)
    {
        SetActorTickEnabled(true);   // driver-synced fade takes over in Tick()
    }
    else
    {
        SnapToState(bTargetCrushActive);   // binary fallback (legacy / no driver)
    }
}

void AkdCrushShadowVolume::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Safety: only the slab path ticks. Anything else disables itself.
    if (!ShadowMID)
    {
        SetActorTickEnabled(false);
        return;
    }

    // BlendAlpha: 0 = light world, 1 = crush world — the exact curve the
    // WorldColorDriver drives the rest of the scene on. Read-only, so the driver
    // stays the sole writer of MPC_WorldColor.
    const float Alpha = CachedColorDriver
        ? CachedColorDriver->GetBlendAlpha()
        : (bTargetCrushActive ? 1.f : 0.f);

    const float Effective = (bVisibleThisCrush ? ShadowOpacity : 0.f) * Alpha;
    ShadowMID->SetScalarParameterValue(MP_ShadowOpacity, Effective);

    // Settle on the frame the world blend finishes: snap to the precise endpoint
    // and idle. Endpoint is derived from the known target, not the sampled alpha,
    // so tick-order against the driver is irrelevant.
    if (!CachedColorDriver || !CachedColorDriver->IsBlending())
    {
        FinalizeFade();
    }
}

void AkdCrushShadowVolume::FinalizeFade()
{
    if (bTargetCrushActive)
    {
        // Fully in Crush Mode.
        if (ShadowMID)
        {
            ShadowMID->SetScalarParameterValue(MP_ShadowOpacity,
                bVisibleThisCrush ? ShadowOpacity : 0.f);
        }
        VolumeMesh->SetVisibility(bVisibleThisCrush);
        VolumeMesh->SetRenderCustomDepth(bVisibleThisCrush && bWriteShadowStencil);
    }
    else
    {
        // Fully back in the lit 3D world — hidden, stencil cleared.
        if (ShadowMID)
        {
            ShadowMID->SetScalarParameterValue(MP_ShadowOpacity, 0.f);
        }
        VolumeMesh->SetVisibility(false);
        VolumeMesh->SetRenderCustomDepth(false);
    }

    SetActorTickEnabled(false);
}
