// Copyright ASKD Games

#include "World/kdShadowZone.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"


// ── FName constants ────────────────────────────────────────────────────────────
// Must match the parameter names in your M_ShadowZone material exactly.
const FName AkdShadowZone::MP_TanHalfHFOV = TEXT("TanHalfHFOV");
const FName AkdShadowZone::MP_ZoneCentreY = TEXT("ZoneCentreY");
const FName AkdShadowZone::MP_ZoneHalfWidth = TEXT("ZoneHalfWidth");
const FName AkdShadowZone::MP_ZoneFadeWidth = TEXT("ZoneFadeWidth");
const FName AkdShadowZone::MP_ZonePlaneX = TEXT("ZonePlaneX");
const FName AkdShadowZone::MP_TopDarkness = TEXT("TopDarkness");
const FName AkdShadowZone::MP_BottomDarkness = TEXT("BottomDarkness");
const FName AkdShadowZone::MP_VertGradExp = TEXT("VertGradExp");
const FName AkdShadowZone::MP_VignetteDark = TEXT("VignetteDark");
const FName AkdShadowZone::MP_TintStrength = TEXT("TintStrength");
const FName AkdShadowZone::MP_ShadowTint = TEXT("ShadowTint");

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

AkdShadowZone::AkdShadowZone()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    // ── Zone bounds box ───────────────────────────────────────────────────────
    // Invisible in-game, blue wireframe in editor.  No collision, no rendering.
    ZoneBoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBoundsBox"));
    SetRootComponent(ZoneBoundsBox);
    ZoneBoundsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ZoneBoundsBox->SetGenerateOverlapEvents(false);
    ZoneBoundsBox->SetVisibility(false);
    ZoneBoundsBox->SetHiddenInGame(true);
    ZoneBoundsBox->ShapeColor = FColor(80, 120, 220);

    // ── Post-process component ────────────────────────────────────────────────
    // bUnbound = true  → always runs globally.
    // BlendWeight = 0  → zero GPU cost until CrushMode activates.
    // Priority = 2     → above base world PPVs (Priority 1), below
    //                    UkdWorldColorDriver (Priority 1000).
    ShadowPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("ShadowPostProcess"));
    ShadowPostProcess->SetupAttachment(RootComponent);
    ShadowPostProcess->bUnbound = true;
    ShadowPostProcess->Priority = 2.f;
    ShadowPostProcess->BlendWeight = 0.f;

    // ── Stencil detection box ─────────────────────────────────────────────────
    // Thin slab — overlap-only.  Stamps ShadowStencilValue on actors that
    // lie on the crush plane X, feeding the neon colour-preserve pipeline.
    StencilDetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("StencilDetectionBox"));
    StencilDetectionBox->SetupAttachment(RootComponent);
    StencilDetectionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    StencilDetectionBox->SetCollisionResponseToAllChannels(ECR_Overlap);
    StencilDetectionBox->SetGenerateOverlapEvents(true);
    StencilDetectionBox->SetHiddenInGame(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void AkdShadowZone::BeginPlay()
{
    Super::BeginPlay();

    InitialiseComponents();

    if (ShadowPostProcessMaterial)
    {
        // Build the DMI so scalar/vector params can be driven at runtime.
        DynMI = UMaterialInstanceDynamic::Create(ShadowPostProcessMaterial, this);

        // Register as a blendable — the renderer picks it up from Settings.
        // Weight = 1: the material is always at full strength when BlendWeight > 0.
        ShadowPostProcess->Settings.WeightedBlendables.Array.Emplace(1.f, DynMI);

        InitialiseMaterialParameters();
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("AkdShadowZone [%s]: ShadowPostProcessMaterial is null. "
                "Assign M_ShadowZone in the Blueprint Details panel under "
                "'Shadow Zone | Material'."),
            *GetName());
    }

    RegisterCrushModeTag();
}

// ─────────────────────────────────────────────────────────────────────────────
// Component sizing
// ─────────────────────────────────────────────────────────────────────────────

void AkdShadowZone::InitialiseComponents()
{
    ZoneBoundsBox->SetBoxExtent(FVector(XHalfExtent, YHalfExtent, ZHalfExtent));

    // Stencil slab: full Y/Z extent, minimal X depth.
    StencilDetectionBox->SetBoxExtent(
        FVector(VolumeThickness * 0.5f, YHalfExtent, ZHalfExtent));
}

// ─────────────────────────────────────────────────────────────────────────────
// Material initialisation (design-time values — written once)
// ─────────────────────────────────────────────────────────────────────────────

void AkdShadowZone::InitialiseMaterialParameters()
{
    if (!DynMI) return;

    // ── World-space zone geometry (static — never change per frame) ───────────
    // Aspect ratio for TanHalfHFOV — read once at BeginPlay.
    float AspectRatio = 16.f / 9.f;
    if (GEngine && GEngine->GameViewport)
    {
        FVector2D VPSize;
        GEngine->GameViewport->GetViewportSize(VPSize);
        if (VPSize.Y > 0.f) AspectRatio = VPSize.X / VPSize.Y;
    }

    // TanHalfHFOV: horizontal half-FOV tangent for the 2D crush camera.
    // Crush2DFOV is the *vertical* FOV (matching UkdCrushTransitionComponent).
    // Multiplying by aspect ratio converts to horizontal.
    const float TanHalfHFOV = FMath::Tan(FMath::DegreesToRadians(Crush2DFOV * 0.5f)) * AspectRatio;

    DynMI->SetScalarParameterValue(MP_TanHalfHFOV, TanHalfHFOV);
    DynMI->SetScalarParameterValue(MP_ZoneCentreY, GetActorLocation().Y);
    DynMI->SetScalarParameterValue(MP_ZoneHalfWidth, YHalfExtent);
    DynMI->SetScalarParameterValue(MP_ZoneFadeWidth, EdgeFadeWorldUnits);
    DynMI->SetScalarParameterValue(MP_ZonePlaneX, GetActorLocation().X);  // = CrushWorldX

    // ── Visual parameters (static) ────────────────────────────────────────────
    DynMI->SetScalarParameterValue(MP_TopDarkness, TopDarkness);
    DynMI->SetScalarParameterValue(MP_BottomDarkness, BottomDarkness);
    DynMI->SetScalarParameterValue(MP_VertGradExp, VertGradExponent);
    DynMI->SetScalarParameterValue(MP_VignetteDark, VignetteDarkness);
    DynMI->SetScalarParameterValue(MP_TintStrength, TintStrength);
    DynMI->SetVectorParameterValue(MP_ShadowTint, ShadowTint);
}

// ─────────────────────────────────────────────────────────────────────────────
// GAS tag registration — identical pattern to AkdShadow2DPlane
// ─────────────────────────────────────────────────────────────────────────────

void AkdShadowZone::RegisterCrushModeTag()
{
    AkdMyPlayer* Player = Cast<AkdMyPlayer>(
        UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

    if (!Player)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("AkdShadowZone [%s]: Local player not found at BeginPlay. "
                "Place this actor in a level that has a valid AkdMyPlayer pawn."),
            *GetName());
        return;
    }

    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    if (!ASC)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("AkdShadowZone [%s]: Player has no ASC."), *GetName());
        return;
    }

    ASC->RegisterGameplayTagEvent(
        FkdGameplayTags::Get().State_CrushMode,
        EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &AkdShadowZone::OnCrushModeTagChanged);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick — fade + per-frame projection update
// ─────────────────────────────────────────────────────────────────────────────

void AkdShadowZone::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // No active fade — nothing to do.  Tick is re-enabled by OnCrushModeTagChanged.
    if (FMath::IsNearlyZero(FadeDirection)) return;

    FadeAlpha += FadeDirection * (DeltaTime / FMath::Max(FadeDuration, KINDA_SMALL_NUMBER));
    FadeAlpha = FMath::Clamp(FadeAlpha, 0.f, 1.f);

    const float Evaluated = FadeAlphaCurve
        ? FadeAlphaCurve->GetFloatValue(FadeAlpha)
        : FadeAlpha;

    ShadowPostProcess->BlendWeight = Evaluated;

    // ── Fade-in complete ──────────────────────────────────────────────────────
    if (FadeDirection > 0.f && FadeAlpha >= 1.f)
    {
        FadeDirection = 0.f;
        SetActorTickEnabled(false);  // World params are static — no per-frame work needed
        SetStencilOnOverlappingActors(true);
        BP_OnShadowFadeInComplete();

#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log, TEXT("ShadowZone [%s]: Fade-in complete."), *GetName());
#endif
    }
    // ── Fade-out complete ─────────────────────────────────────────────────────
    else if (FadeDirection < 0.f && FadeAlpha <= 0.f)
    {
        FadeDirection = 0.f;
        SetActorTickEnabled(false);
        SetStencilOnOverlappingActors(false);
        SetVolumeOverlapEventsActive(false);
        BP_OnShadowFadeOutComplete();

#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log, TEXT("ShadowZone [%s]: Fade-out complete."), *GetName());
#endif
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// GAS tag callback
// ─────────────────────────────────────────────────────────────────────────────

void AkdShadowZone::OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    const bool bEntering = NewCount > 0;

    if (bEntering && !bCrushActive)
    {
        bCrushActive = true;
        FadeDirection = 1.f;
        // FadeAlpha continues from its current value — supports interrupted fade-outs.
        SetVolumeOverlapEventsActive(true);
        SetActorTickEnabled(true);

#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log, TEXT("ShadowZone [%s]: Fading IN  (alpha = %.2f)."), *GetName(), FadeAlpha);
#endif
    }
    else if (!bEntering && bCrushActive)
    {
        bCrushActive = false;
        FadeDirection = -1.f;
        // Clear stencil immediately on exit — don't wait for fade-out to complete.
        SetStencilOnOverlappingActors(false);
        SetActorTickEnabled(true);

#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log, TEXT("ShadowZone [%s]: Fading OUT (alpha = %.2f)."), *GetName(), FadeAlpha);
#endif
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Stencil helpers — identical contract to AkdShadow2DPlane
// ─────────────────────────────────────────────────────────────────────────────

void AkdShadowZone::SetVolumeOverlapEventsActive(bool bActive)
{
    if (bActive && !bOverlapBound)
    {
        StencilDetectionBox->OnComponentBeginOverlap.AddDynamic(
            this, &AkdShadowZone::OnActorEnterStencilVolume);
        StencilDetectionBox->OnComponentEndOverlap.AddDynamic(
            this, &AkdShadowZone::OnActorExitStencilVolume);
        bOverlapBound = true;
    }
    else if (!bActive && bOverlapBound)
    {
        StencilDetectionBox->OnComponentBeginOverlap.RemoveDynamic(
            this, &AkdShadowZone::OnActorEnterStencilVolume);
        StencilDetectionBox->OnComponentEndOverlap.RemoveDynamic(
            this, &AkdShadowZone::OnActorExitStencilVolume);
        bOverlapBound = false;
    }
}

void AkdShadowZone::SetStencilOnOverlappingActors(bool bEnable)
{
    TArray<AActor*> Overlapping;
    StencilDetectionBox->GetOverlappingActors(Overlapping);
    for (AActor* Actor : Overlapping)
    {
        if (IsValid(Actor)) SetActorShadowStencil(Actor, bEnable);
    }
}

void AkdShadowZone::SetActorShadowStencil(AActor* TargetActor, bool bEnable) const
{
    if (!IsValid(TargetActor)) return;

    TArray<UPrimitiveComponent*> Primitives;
    TargetActor->GetComponents<UPrimitiveComponent>(Primitives);

    for (UPrimitiveComponent* Prim : Primitives)
    {
        if (!Prim) continue;
        // SetRenderCustomDepth must precede stencil value assignment — otherwise
        // the stencil write is silently skipped by the renderer.
        Prim->SetRenderCustomDepth(bEnable);
        if (bEnable) Prim->SetCustomDepthStencilValue(ShadowStencilValue);
    }
}

void AkdShadowZone::OnActorEnterStencilVolume(UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    // Only stamp stencil after fully faded in — prevents the neon PP effect
    // from popping on before the shadow is fully visible.
    if (bCrushActive && FadeAlpha >= 1.f && IsValid(OtherActor))
    {
        SetActorShadowStencil(OtherActor, true);
    }
}

void AkdShadowZone::OnActorExitStencilVolume(UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    // Always clear stencil on exit so no actor is left with a stale value.
    if (IsValid(OtherActor)) SetActorShadowStencil(OtherActor, false);
}
