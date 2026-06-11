// Copyright ASKD Games


#include "Components/kdGeometryTransitionComponent.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"



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

    MeshRelScaleX_Original = OriginalMeshRelativeScale.X;
    MeshRelScaleX_Crush = OriginalMeshRelativeScale.X * CrushXScaleMultiplier;

    // Remember the designer's shadow settings so we can restore them on Crush exit.
    bOrigCastShadow = (CachedMesh->CastShadow != 0);
    bOrigCastHiddenShadow = (CachedMesh->bCastHiddenShadow != 0);

    // ── Compute the relative X offset that places the mesh at the crush plane ──
    //
    // mesh world X ≈ actorWorldX + relativeX (assuming no actor rotation on X)
    // We want mesh world X = CrushWorldX + behind-offset
    // → relativeX_crush = (CrushWorldX + offset) - actorWorldX
    //
    // This is an approximation valid for unrotated actors (standard for geometry).
    // For rotated actors the math extends to a full inverse-transform, but that
    // case is uncommon enough that we document it as "not supported with rotation".
    const float ActorWorldX = Owner->GetActorLocation().X;
    MeshRelX_Original = OriginalMeshRelativeLoc.X;
    MeshRelX_Crush = MeshRelX_Original + ((CrushWorldX + ResolveDepthOffset(CrushWorldX)) - ActorWorldX);

    // ── Register on the player's ASC ──────────────────────────────────────────
    AkdMyPlayer* Player = Cast<AkdMyPlayer>(
        UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

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
        FVector SnapLoc = OriginalMeshRelativeLoc;
        SnapLoc.X = MeshRelX_Crush;
        FVector SnapScale = OriginalMeshRelativeScale;
        SnapScale.X = MeshRelScaleX_Crush;
        CachedMesh->SetRelativeLocation(SnapLoc);
        CachedMesh->SetRelativeScale3D(SnapScale);

        // Flatten the shadow too if we boot straight into Crush mode.
        bToCrushMode = true;
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
        // Ease-in-out cubic so the slide starts slow, accelerates, then eases in.
        const float t = FMath::Clamp(StateElapsed / FMath::Max(MorphDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
        const float Alpha = t * t * (3.f - 2.f * t);   // smoothstep

        // Resolve FROM and TO directions.
        const float FromRelX = bToCrushMode ? MeshRelX_Original : MeshRelX_Crush;
        const float ToRelX = bToCrushMode ? MeshRelX_Crush : MeshRelX_Original;
        const float FromScaleX = bToCrushMode ? MeshRelScaleX_Original : MeshRelScaleX_Crush;
        const float ToScaleX = bToCrushMode ? MeshRelScaleX_Crush : MeshRelScaleX_Original;

        // ── X position lerp (slide onto / off the shadow plane) ───────────────
        FVector NewRelLoc = OriginalMeshRelativeLoc;
        NewRelLoc.X = FMath::Lerp(FromRelX, ToRelX, Alpha);
        CachedMesh->SetRelativeLocation(NewRelLoc);

        // ── X scale lerp (squash / unsquash) ──────────────────────────────────
        FVector NewRelScale = OriginalMeshRelativeScale;
        NewRelScale.X = FMath::Lerp(FromScaleX, ToScaleX, Alpha);
        CachedMesh->SetRelativeScale3D(NewRelScale);

        if (Alpha >= 1.f)
        {
            // Hard-snap to exact target — eliminates floating-point residual.
            NewRelLoc.X = ToRelX;
            NewRelScale.X = ToScaleX;
            CachedMesh->SetRelativeLocation(NewRelLoc);
            CachedMesh->SetRelativeScale3D(NewRelScale);

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

    // ── Recalculate crush target from the player's current X ─────────────────
    // Runs at the moment of toggle so every crush snaps geometry to wherever
    // the player actually is — not a designer-set constant.
    if (bToCrushMode)
    {
        AkdMyPlayer* Player = Cast<AkdMyPlayer>(
            UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

        if (Player)
        {
            const float PlayerWorldX = Player->GetActorLocation().X;
            const float ActorWorldX = GetOwner()->GetActorLocation().X;

            // Convert player world X (+ behind-offset) into a mesh-relative X delta.
            // mesh world X ≈ actorWorldX + relativeX (valid for unrotated actors).
            // CrushDepthOffset is 0 for floors/shadow areas and >0 for walls.
            const float AppliedOffset = ResolveDepthOffset(PlayerWorldX);
            MeshRelX_Crush = MeshRelX_Original + ((PlayerWorldX + AppliedOffset) - ActorWorldX);
            MeshRelScaleX_Crush = OriginalMeshRelativeScale.X * CrushXScaleMultiplier;

#if !UE_BUILD_SHIPPING
            UE_LOG(LogTemp, Log,
                TEXT("GeometryTransition [%s]: CrushTarget X = %.1f (playerWorldX=%.1f, offset=%.1f)"),
                *GetOwner()->GetName(), PlayerWorldX + AppliedOffset, PlayerWorldX, AppliedOffset);
#endif
        }
    }

    // Flatten the cast shadow on the way in, restore it on the way out.
    ApplyShadowFlatten(bToCrushMode);

    StateElapsed = 0.f;
    State = (ShiverDuration > KINDA_SMALL_NUMBER) ? EGeoState::Shivering : EGeoState::Morphing;
    SetComponentTickEnabled(true);
}

float UkdGeometryTransitionComponent::ResolveDepthOffset(float ReferenceX) const
{
    // Manual mode: use the raw signed value the designer entered.
    if (!bAutoOffsetBehindCamera)
    {
        return CrushDepthOffset;
    }

    // Auto mode: push away from the Crush camera so a positive magnitude is
    // always "behind" the player, regardless of which side the camera sits on.
    if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
    {
        const float CamX = Cam->GetCameraLocation().X;
        const float BehindSign = (CamX >= ReferenceX) ? -1.f : 1.f;
        return FMath::Abs(CrushDepthOffset) * BehindSign;
    }

    // Camera not ready yet (e.g. boot-into-crush) — fall back to the raw value.
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
