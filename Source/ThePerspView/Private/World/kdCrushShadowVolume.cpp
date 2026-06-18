// Copyright ASKD Games

#include "World/kdCrushShadowVolume.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameplayTags/kdGameplayTags.h"        
#include "Player/kdMyPlayer.h"
#include "Crush/kdCrushDirectionLibrary.h"


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

    // Custom-Depth-ONLY: never drawn in colour, casts nothing, collides with
    // nothing. Its footprint is the only thing we use.
    VolumeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VolumeMesh->SetCanEverAffectNavigation(false);
    VolumeMesh->SetCastShadow(false);
    VolumeMesh->bRenderInMainPass = false;                 // invisible in the scene
    VolumeMesh->SetRenderCustomDepth(false);               // enabled only in Crush Mode
    VolumeMesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
}

void AkdCrushShadowVolume::ApplyZoneScale(bool bCollapsesY)
{
    // Engine cube is 100 cm; scale each axis to the requested world size.
    //VolumeMesh->SetWorldScale3D(FVector(
    //    ZoneDepthX / 100.f,
    //    ZoneWidthY / 100.f,
    //    ZoneHeightZ / 100.f));

    // Thin depth on the collapse axis; wide on the in-plane horizontal axis.
    const FVector Scale = bCollapsesY
        ? FVector(ZoneWidthY / 100.f, ZoneDepthX / 100.f, ZoneHeightZ / 100.f)  // Y-crush: thin on Y
        : FVector(ZoneDepthX / 100.f, ZoneWidthY / 100.f, ZoneHeightZ / 100.f); // X-crush: thin on X
    VolumeMesh->SetWorldScale3D(Scale);
}

void AkdCrushShadowVolume::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("ShadowVolume BeginPlay"));

    OriginalPlacedLocation = GetActorLocation();

    ApplyZoneScale(false);

    VolumeMesh->SetRenderInMainPass(false);              // invisible in colour
    VolumeMesh->SetCastShadow(false);                    // <-- THE FIX: no shadow from the box
    VolumeMesh->bCastHiddenShadow = false;               // belt-and-suspenders for hidden-mesh shadows
    VolumeMesh->SetRenderCustomDepth(true);              // still writes the stencil tag
    VolumeMesh->SetCustomDepthStencilValue(ShadowStencilValue);

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

void AkdCrushShadowVolume::OnCrushModeTagChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
    SetVolumeActive(NewCount > 0);
}

void AkdCrushShadowVolume::SetVolumeActive(bool bActive)
{
    //if (bActive && bTrackPlayerDepth && CachedPlayer)
    //{
    //    // Camera looks down +X from a long negative-X spring arm, so a larger X
    //    // is "further from camera". Keeping the slab's NEAR face just behind the
    //    // player guarantees the player's stencil-1 wins → player stays lit.
    //    const float PlayerX = CachedPlayer->GetActorLocation().X;
    //    FVector Loc = GetActorLocation();
    //    Loc.X = PlayerX + DepthBehindPlayer + (ZoneDepthX * 0.5f);
    //    SetActorLocation(Loc);
    //}

    //VolumeMesh->SetRenderCustomDepth(bActive);
    //VolumeMesh->SetVisibility(bActive);

    if (bActive && CachedPlayer)
    {
        const FkdCrushBasis Basis = UkdCrushDirectionLibrary::MakeCrushBasis(
            CachedPlayer->GetActiveCrushDirection());

        // Orient the slab for this crush direction.
        ApplyZoneScale(Basis.bCollapsesY);

        if (bTrackPlayerDepth)
        {
            const FVector PlayerLoc = CachedPlayer->GetActorLocation();
            const float   Behind = DepthBehindPlayer + (ZoneDepthX * 0.5f);

            // Rebuild from the authored placement, NOT the current (possibly
            // drifted) location. Only the depth axis follows the player's plane.
            FVector Loc = OriginalPlacedLocation;   // ← was GetActorLocation()

            if (Basis.bCollapsesY)
                Loc.Y = PlayerLoc.Y + Basis.ViewForward.Y * Behind;
            else
                Loc.X = PlayerLoc.X + Basis.ViewForward.X * Behind;

            SetActorLocation(Loc);
        }
    }

    VolumeMesh->SetRenderCustomDepth(bActive);
    VolumeMesh->SetVisibility(bActive);
}