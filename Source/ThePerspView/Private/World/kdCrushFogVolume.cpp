// Copyright ASKD Games

#include "World/kdCrushFogVolume.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/BoxComponent.h"
#include "Curves/CurveFloat.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Kismet/GameplayStatics.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

AkdCrushFogVolume::AkdCrushFogVolume()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;   // only ticks during fades

    // ── Fog volume (root) ──────────────────────────────────────────────────────
    FogVolume = CreateDefaultSubobject<ULocalFogVolumeComponent>(TEXT("FogVolume"));
    SetRootComponent(FogVolume);

    // Start invisible.  Runtime changes (post-registration) MUST go through
    // the setter functions — direct writes here in the ctor are fine because
    // the render proxy hasn't been created yet.
    FogVolume->RadialFogExtinction = 0.f;
    FogVolume->HeightFogExtinction = 0.f;   // disable height-based fog component;
    // slab shape is fully actor-scale driven

// ── Editor bounds visualizer ───────────────────────────────────────────────
// BoxExtent = GetBaseVolumeSize() (500 cm) matches the component's immutable
// base sphere radius.  Combined with the actor scale applied in BeginPlay,
// the box outline draws exactly over the fog ellipsoid in the viewport.
    BoundsViz = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsViz"));
    BoundsViz->SetupAttachment(RootComponent);
    BoundsViz->SetBoxExtent(FVector(ULocalFogVolumeComponent::GetBaseVolumeSize()));
    BoundsViz->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BoundsViz->SetHiddenInGame(true);
    BoundsViz->ShapeColor = FColor(80, 30, 160);   // indigo — matches shadow aesthetic
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void AkdCrushFogVolume::BeginPlay()
{
    Super::BeginPlay();

    // ── Shape the slab from designer extents ───────────────────────────────────
    SizeVolumeFromExtents();

    // ── Static fog properties (set once via setters — never mutated per-tick) ──
    // Setter functions dispatch to the render thread proxy.  Direct property
    // writes on a registered component would not propagate to the renderer.
    FogVolume->SetFogAlbedo(FogAlbedoColor);
    FogVolume->SetFogEmissive(FogEmissiveColor);
    FogVolume->SetFogPhaseG(ScatteringPhaseG);

    // Ensure height fog contribution is zero — shadow slab is radial-only.
    FogVolume->SetHeightFogExtinction(0.f);

    // ── GAS tag subscription ───────────────────────────────────────────────────
    // Mirrors AkdShadowPortal / AkdShadowEnemy pattern — zero per-frame polling.
    CachedPlayer = Cast<AkdMyPlayer>(
        UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

    if (!CachedPlayer)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("AkdCrushFogVolume [%s]: Local player not found at BeginPlay. "
                "Ensure AkdMyPlayer is spawned before this actor's BeginPlay."),
            *GetName());
        return;
    }

    UAbilitySystemComponent* ASC = CachedPlayer->GetAbilitySystemComponent();
    if (!ASC) return;

    CrushTagHandle = ASC->RegisterGameplayTagEvent(
        FkdGameplayTags::Get().State_CrushMode,
        EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &AkdCrushFogVolume::OnCrushModeTagChanged);

    // ── Sync if the level starts already in CrushMode ─────────────────────────
    if (ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode))
    {
        // Apply the same X-repositioning that OnCrushModeTagChanged would do.
        FVector NewLoc = GetActorLocation();
        NewLoc.X = CachedPlayer->GetActorLocation().X;
        SetActorLocation(NewLoc);

        bCrushActive = true;
        CurrentAlpha = 1.f;
        ApplyFogExtinction(1.f);

#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log,
            TEXT("AkdCrushFogVolume [%s]: Started in CrushMode at X=%.1f — fog set to peak extinction."),
            *GetName(), GetActorLocation().X);
#endif
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick  (only active while a fade is in progress)
// ─────────────────────────────────────────────────────────────────────────────

void AkdCrushFogVolume::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (FMath::IsNearlyZero(FadeDirection)) return;

    // Advance normalised alpha toward 0 or 1
    CurrentAlpha = FMath::Clamp(
        CurrentAlpha + FadeDirection * (DeltaTime / FadeDuration),
        0.f, 1.f);

    // Optional designer curve remaps linear time to a custom easing shape.
    // Null → linear fallback.  Curve is evaluated HERE — ApplyFogExtinction
    // receives the final value and must not call GetFloatValue again.
    const float Evaluated = FadeCurve
        ? FadeCurve->GetFloatValue(CurrentAlpha)
        : CurrentAlpha;

    ApplyFogExtinction(Evaluated);

    // ── Fade-in complete ───────────────────────────────────────────────────────
    if (FadeDirection > 0.f && CurrentAlpha >= 1.f)
    {
        FadeDirection = 0.f;
        SetActorTickEnabled(false);
        BP_OnFogFadeInComplete();

#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log,
            TEXT("AkdCrushFogVolume [%s]: Fade-in complete (RadialExtinction=%.4f)."),
            *GetName(), FogVolume->RadialFogExtinction);
#endif
    }
    // ── Fade-out complete ──────────────────────────────────────────────────────
    else if (FadeDirection < 0.f && CurrentAlpha <= 0.f)
    {
        FadeDirection = 0.f;
        SetActorTickEnabled(false);
        BP_OnFogFadeOutComplete();

#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log,
            TEXT("AkdCrushFogVolume [%s]: Fade-out complete."), *GetName());
#endif
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// EndPlay
// ─────────────────────────────────────────────────────────────────────────────

void AkdCrushFogVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Unregister the GAS tag delegate so the ASC doesn't hold a dangling
    // pointer to this actor after it is destroyed.
    AkdMyPlayer* Player = Cast<AkdMyPlayer>(
        UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

    if (Player)
    {
        if (UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent())
        {
            ASC->RegisterGameplayTagEvent(
                FkdGameplayTags::Get().State_CrushMode,
                EGameplayTagEventType::NewOrRemoved)
                .Remove(CrushTagHandle);
        }
    }

    Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

void AkdCrushFogVolume::SizeVolumeFromExtents()
{
    // ULocalFogVolumeComponent is a unit sphere whose radius is fixed at
    // GetBaseVolumeSize() = 500 cm.  Dividing each half-extent by 500 gives
    // the per-axis scale multiplier that stretches the sphere into the slab.
    //
    // Example with defaults (all values in cm):
    //   X: 80   / 500 = 0.16  →  160 cm total depth  (tight shadow wall)
    //   Y: 3000 / 500 = 6.00  →  6000 cm total width (full level width)
    //   Z: 2000 / 500 = 4.00  →  4000 cm total height (floor-to-ceiling)
    //
    // BoundsViz (BoxExtent=500) inherits actor scale automatically —
    // no extra sizing needed; it will outline the slab precisely in editor.
    const float Base = ULocalFogVolumeComponent::GetBaseVolumeSize();
    SetActorScale3D(FVector(VolumeHalfExtentX / Base, VolumeHalfExtentY / Base, VolumeHalfExtentZ / Base));
}

void AkdCrushFogVolume::ApplyFogExtinction(float EvaluatedAlpha)
{
    // SetRadialFogExtinction is the correct runtime setter — it dispatches the
    // new value to the render proxy via SendRenderTransformCommand.
    // Do NOT call MarkRenderStateDirty here; the setter handles propagation.
    FogVolume->SetRadialFogExtinction(EvaluatedAlpha * PeakRadialExtinction);
}

void AkdCrushFogVolume::OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    const bool bEntering = NewCount > 0;

    if (bEntering && !bCrushActive)
    {
        // ── Reposition the slab to the player's current shadow-plane X ─────────
        // The geometry slides to playerWorldX on every Crush entry (varying each
        // time), so the fog must teleport to that same X before fading in.
        // Y and Z stay as placed in the editor — they only need to cover the
        // playfield width / floor-ceiling, which doesn't change per-entry.
        if (IsValid(CachedPlayer))
        {
            FVector NewLoc = GetActorLocation();
            NewLoc.X = CachedPlayer->GetActorLocation().X;
            SetActorLocation(NewLoc);
        }

        bCrushActive = true;
        FogVolume->HeightFogExtinction = 2.0f;
        FadeDirection = 1.f;
        SetActorTickEnabled(true);

#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log,
            TEXT("AkdCrushFogVolume [%s]: Fading IN at X=%.1f"),
            *GetName(), GetActorLocation().X);
#endif
    }
    else if (!bEntering && bCrushActive)
    {
        bCrushActive = false;
        FadeDirection = -1.f;
        FogVolume->HeightFogExtinction = 0.0f;
        SetActorTickEnabled(true);

#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log,
            TEXT("AkdCrushFogVolume [%s]: Fading OUT"), *GetName());
#endif
    }
}
