// Copyright ASKD Games


#include "Components/kdGeometryTransitionComponent.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Crush/kdCrushDirectionLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"


UkdGeometryTransitionComponent::UkdGeometryTransitionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UkdGeometryTransitionComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (!Owner) return;

    // ── Find the first StaticMeshComponent ────────────────────────────────────
    CachedMesh = Owner->FindComponentByClass<UStaticMeshComponent>();
    if (!CachedMesh)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("WorldGeometryTransition [%s]: No UStaticMeshComponent found — disabled."),
            *Owner->GetName());
        return;
    }

    // ── Capture mesh-relative baselines ───────────────────────────────────────
    // We store the mesh's OWN relative transform (relative to actor root).
    // This is independent of the actor's mobility — we never touch SetActorLocation.
    OriginalMeshRelativeLoc = CachedMesh->GetRelativeLocation();
    OriginalMeshRelativeScale = CachedMesh->GetRelativeScale3D();

    // Remember the designer's shadow settings so we can restore them on Crush exit.
    bOrigCastShadow = (CachedMesh->CastShadow != 0);
    bOrigCastHiddenShadow = (CachedMesh->bCastHiddenShadow != 0);

    // ── Register on the player's ASC ──────────────────────────────────────────
    AkdMyPlayer* Player = Cast<AkdMyPlayer>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (!Player)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("WorldGeometryTransition [%s]: No AkdMyPlayer found — inactive."),
            *Owner->GetName());
        return;
    }

    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    if (!ASC) return;

    ASC->RegisterGameplayTagEvent(
        FkdGameplayTags::Get().State_CrushMode,
        EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &UkdGeometryTransitionComponent::OnCrushModeTagChanged);

    // Snap immediately if the level starts already in Crush mode.
    if (ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode))
    {
        bToCrushMode = true;
        RecomputeCrushTarget();   // uses the player's current active direction
        CachedMesh->SetRelativeLocation(MeshRelLocCrush);
        CachedMesh->SetRelativeScale3D(MeshRelScaleCrush);
        ApplyShadowFlatten(true);
    }

}

void UkdGeometryTransitionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedMesh) { SetComponentTickEnabled(false); return; }

    StateElapsed += DeltaTime;

    switch (State)
    {
        // ── SHIVER ────────────────────────────────────────────────────────────────
        // Rapid Y/Z oscillation with amplitude decaying to zero by ShiverDuration.
        // Mesh snaps back to its clean relative origin each frame + the wave offset,
        // so shiver never drifts or accumulates error across frames.
    case EGeoState::Shivering:
    {
        const float t = StateElapsed / FMath::Max(ShiverDuration, KINDA_SMALL_NUMBER);
        const float Decay = 1.f - FMath::Clamp(t, 0.f, 1.f);
        const float Wave = FMath::Sin(StateElapsed * ShiverFrequency * 2.f * PI);
        const float Disp = ShiverAmplitude * Decay * Wave;

        FVector ShiverLoc = OriginalMeshRelativeLoc;
        ShiverLoc.Y += Disp;
        ShiverLoc.Z += Disp * 0.45f;   // different amplitude on Z for character

        CachedMesh->SetRelativeLocation(ShiverLoc);

        if (t >= 1.f)
        {
            // Restore clean origin before morph begins.
            CachedMesh->SetRelativeLocation(OriginalMeshRelativeLoc);
            State = EGeoState::Morphing;
            StateElapsed = 0.f;
        }
        break;
    }

    // ── MORPH ─────────────────────────────────────────────────────────────────
    // Simultaneously shifts the mesh's X relative position and X scale.
    // This is what actually moves the geometry to the shadow plane visually.
    case EGeoState::Morphing:
    {
        const float t = FMath::Clamp(StateElapsed / FMath::Max(MorphDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
        const float Alpha = t * t * (3.f - 2.f * t);   // smoothstep

        // Only the collapse-axis component differs between original and crush,
        // so lerping the whole vector leaves the other axes untouched.
        const FVector FromLoc = bToCrushMode ? OriginalMeshRelativeLoc : MeshRelLocCrush;
        const FVector ToLoc = bToCrushMode ? MeshRelLocCrush : OriginalMeshRelativeLoc;
        const FVector FromScale = bToCrushMode ? OriginalMeshRelativeScale : MeshRelScaleCrush;
        const FVector ToScale = bToCrushMode ? MeshRelScaleCrush : OriginalMeshRelativeScale;

        CachedMesh->SetRelativeLocation(FMath::Lerp(FromLoc, ToLoc, Alpha));
        CachedMesh->SetRelativeScale3D(FMath::Lerp(FromScale, ToScale, Alpha));

        if (Alpha >= 1.f)
        {
            CachedMesh->SetRelativeLocation(ToLoc);   // hard-snap, kill FP residual
            CachedMesh->SetRelativeScale3D(ToScale);
            State = EGeoState::Idle;
            SetComponentTickEnabled(false);
        }
        break;
    }

    default:
        SetComponentTickEnabled(false);
        break;
    }
}

void UkdGeometryTransitionComponent::OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (!CachedMesh) return;

    bToCrushMode = (NewCount > 0);

    // Recompute the target on entry so it snaps to the player's current position
    // on whatever axis the active direction collapses. Exit reuses the entry target.
    if (bToCrushMode)
    {
        RecomputeCrushTarget();
    }

    ApplyShadowFlatten(bToCrushMode);

    StateElapsed = 0.f;
    State = (ShiverDuration > KINDA_SMALL_NUMBER) ? EGeoState::Shivering : EGeoState::Morphing;    
    SetComponentTickEnabled(true);
}

float UkdGeometryTransitionComponent::ResolveDepthOffset(const FVector& CollapseNormal, float ReferenceOnAxis) const
{
    if (!bAutoOffsetBehindCamera)
    {
        return CrushDepthOffset;
    }

    // Push away from the Crush camera along the collapse axis, so a positive
    // magnitude is always "behind", regardless of which side the camera sits.
    if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
    {
        const float CamOnAxis = FVector::DotProduct(Cam->GetCameraLocation(), CollapseNormal);
        const float BehindSign = (CamOnAxis >= ReferenceOnAxis) ? -1.f : 1.f;
        return FMath::Abs(CrushDepthOffset) * BehindSign;
    }

    return CrushDepthOffset;
}

void UkdGeometryTransitionComponent::ApplyShadowFlatten(bool bFlatten)
{
    if (!bFlattenShadowInCrush || !CachedMesh) return;

    if (bFlatten)
    {
        // Suppress 3D shadow casting so the flattened slab doesn't read as a
        // volumetric shadow in the side view — the flat look comes from the
        // projected position and the stencil post-process.
        CachedMesh->SetCastShadow(false);
        CachedMesh->bCastHiddenShadow = false;
    }
    else
    {
        // Restore the designer's original settings (never hard-code true).
        CachedMesh->SetCastShadow(bOrigCastShadow);
        CachedMesh->bCastHiddenShadow = bOrigCastHiddenShadow;
    }
    CachedMesh->MarkRenderStateDirty();
}

void UkdGeometryTransitionComponent::RecomputeCrushTarget()
{
    AkdMyPlayer* Player = Cast<AkdMyPlayer>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (!Player) return;

    // Which axis collapses for the player's active crush direction.
    const FkdCrushBasis Basis = UkdCrushDirectionLibrary::MakeCrushBasis(Player->GetActiveCrushDirection());
    const FVector CollapseN = Basis.CollapseNormal;  // (1,0,0) for N/S, (0,1,0) for E/W
    const bool    bY = Basis.bCollapsesY;

    // Project player, actor, and mesh origin onto the collapse axis.
    const float PlayerOnAxis = bY ? Player->GetActorLocation().Y : Player->GetActorLocation().X;
    const float ActorOnAxis = bY ? GetOwner()->GetActorLocation().Y : GetOwner()->GetActorLocation().X;
    const float OrigRelOnAxis = bY ? OriginalMeshRelativeLoc.Y : OriginalMeshRelativeLoc.X;
    const float OrigScaleOnAxis = bY ? OriginalMeshRelativeScale.Y : OriginalMeshRelativeScale.X;

    // Push behind the player along the collapse axis (0 for floors, >0 for walls).
    const float AppliedOffset = ResolveDepthOffset(CollapseN, PlayerOnAxis);

    // Mesh-relative target on the collapse axis so mesh world-on-axis = player + offset.
    // (Valid for unrotated actors — same assumption the X version always made.)
    const float CrushRelOnAxis = OrigRelOnAxis + ((PlayerOnAxis + AppliedOffset) - ActorOnAxis);  //<-- Old

    // Build the full target vectors: originals everywhere, override only the collapse axis.
    MeshRelLocCrush = OriginalMeshRelativeLoc;
    MeshRelScaleCrush = OriginalMeshRelativeScale;

    if (bY)
    {
        MeshRelLocCrush.Y = CrushRelOnAxis;
        MeshRelScaleCrush.Y = OrigScaleOnAxis * CrushXScaleMultiplier; // name kept; applies to collapse axis
    }
    else
    {
        MeshRelLocCrush.X = CrushRelOnAxis;
        MeshRelScaleCrush.X = OrigScaleOnAxis * CrushXScaleMultiplier;
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log,
        TEXT("GeometryTransition [%s]: CrushTarget %s = %.1f (playerOnAxis=%.1f, offset=%.1f)"),
        *GetOwner()->GetName(), bY ? TEXT("Y") : TEXT("X"),
        PlayerOnAxis + AppliedOffset, PlayerOnAxis, AppliedOffset);
#endif
}
