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

// Material parameter names — must match the M_CrushShadowVolume graph.
const FName AkdCrushShadowVolume::MP_ShadowColor = TEXT("ShadowColor");
const FName AkdCrushShadowVolume::MP_ShadowOpacity = TEXT("ShadowOpacity");


AkdCrushShadowVolume::AkdCrushShadowVolume()
{
    PrimaryActorTick.bCanEverTick = false;

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
        // It remains hidden until Crush Mode activates it (SetVolumeActive).
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

    // Honour current state in case we begin already in Crush Mode.
    SetVolumeActive(CachedASC->GetTagCount(FkdGameplayTags::Get().State_CrushMode) > 0);
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

    // Immediate feedback while tuning in a running PIE session. Outside PIE the slab
    // is not visible anyway (Crush Mode is a runtime state), and the values are
    // re-read on the next activation regardless.
    RefreshMaterialParameters();
}
#endif

void AkdCrushShadowVolume::OnCrushModeTagChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
    SetVolumeActive(NewCount > 0);
}

void AkdCrushShadowVolume::SetVolumeActive(bool bActive)
{
    if (bActive && CachedPlayer)
    {
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
            bActive = false;   // this volume sits out the current crush direction
        }
        else
        {
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

            // Re-read the (possibly edited) tint each time the volume comes alive,
            // giving a reliable edit-in-Details → toggle-crush → see-it loop.
            RefreshMaterialParameters();
        }
    }

    // Stencil write can be suppressed independently of visibility (bWriteShadowStencil).
    VolumeMesh->SetRenderCustomDepth(bActive && bWriteShadowStencil);

    // Drives both the legacy invisible stamp and the translucent tint slab.
    VolumeMesh->SetVisibility(bActive);
}
