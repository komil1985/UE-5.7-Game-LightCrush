// Copyright ASKD Games


#include "Components/kdGeometryTransitionComponent.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Kismet/GameplayStatics.h"



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

    // ── Compute the relative X offset that places the mesh at CrushWorldX ─────
    //
    // mesh world X ≈ actorWorldX + relativeX (assuming no actor rotation on X)
    // We want mesh world X = CrushWorldX
    // → relativeX_crush = CrushWorldX - actorWorldX
    //
    // This is an approximation valid for unrotated actors (standard for geometry).
    // For rotated actors the math extends to a full inverse-transform, but that
    // case is uncommon enough that we document it as "not supported with rotation".
    const float ActorWorldX = Owner->GetActorLocation().X;
    MeshRelX_Original = OriginalMeshRelativeLoc.X;
    MeshRelX_Crush = MeshRelX_Original + (CrushWorldX - ActorWorldX);

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
    StateElapsed = 0.f;
    State = (ShiverDuration > KINDA_SMALL_NUMBER) ? EGeoState::Shivering : EGeoState::Morphing;

    SetComponentTickEnabled(true);
}
