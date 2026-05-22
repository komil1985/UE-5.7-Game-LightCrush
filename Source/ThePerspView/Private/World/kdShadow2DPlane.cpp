// Copyright ASKD Games


#include "World/kdShadow2DPlane.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Player/kdMyPlayer.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/kdGameplayTags.h"
#include "Kismet/GameplayStatics.h"

AkdShadow2DPlane::AkdShadow2DPlane()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

    // ── Plane mesh ────────────────────────────────────────────────────────────
    // The engine's built-in 1 m × 1 m plane.  Scaled to PlaneWidth × PlaneHeight
    // in BeginPlay once we know the designer values.
    ShadowPlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShadowPlaneMesh"));
    SetRootComponent(ShadowPlaneMesh);

    ShadowPlaneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ShadowPlaneMesh->SetCastShadow(false);          // shadow plane must not cast real shadows
    ShadowPlaneMesh->SetReceivesDecals(false);
    ShadowPlaneMesh->SetVisibility(false);           // hidden until first CrushMode entry

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshRef(TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (PlaneMeshRef.Succeeded())
    {
        ShadowPlaneMesh->SetStaticMesh(PlaneMeshRef.Object);
    }

    // ── Stencil overlap volume ────────────────────────────────────────────────
    // Overlap-only box — no blocking collision.  Scaled to PlaneWidth × PlaneHeight
    // with a shallow X depth (VolumeThickness) so only actors truly on the shadow
    // plane receive the stencil, preventing false positives in the 3D world.
    ShadowVolumeBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ShadowVolumeBox"));
    ShadowVolumeBox->SetupAttachment(RootComponent);
    ShadowVolumeBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ShadowVolumeBox->SetCollisionResponseToAllChannels(ECR_Overlap);
    ShadowVolumeBox->SetGenerateOverlapEvents(true);
}

void AkdShadow2DPlane::BeginPlay()
{
	Super::BeginPlay();
	
    // ── Size the plane mesh ───────────────────────────────────────────────────
    // The engine Plane mesh is 100 × 100 cm and lies flat (XY).
    // We rotate it 90° around Y so it faces along X (stands vertically),
    // then scale Width and Height axes to cover the full playfield.
    ShadowPlaneMesh->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
    ShadowPlaneMesh->SetRelativeScale3D(
        FVector(1.f,                        // X — stays thin (1 × 100 cm = 1 m depth)
            PlaneWidth / 100.f,        // Y → world Y covers full level width
            PlaneHeight / 100.f));      // Z → world Z covers floor-to-ceiling

    // ── Size the stencil detection volume ─────────────────────────────────────
    ShadowVolumeBox->SetBoxExtent(
        FVector(VolumeThickness * 0.5f,
            PlaneWidth * 0.5f,
            PlaneHeight * 0.5f));

    // ── Create dynamic material instance ─────────────────────────────────────
    // Slot 0 must contain a BLEND_Modulate Unlit material with scalar parameter
    // "ShadowOpacity" and vector parameter "ShadowColor".  See header for recipe.
    if (UMaterialInterface* BaseMat = ShadowPlaneMesh->GetMaterial(0))
    {
        DynMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
        ShadowPlaneMesh->SetMaterial(0, DynMaterial);
        PushMaterialParameters(0.f); // initialise fully transparent
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("AkdShadow2DPlane [%s]: No material in slot 0. "
                "Assign M_ShadowPlane (BLEND_Modulate) in the Blueprint."),
            *GetName());
    }

    // ── Register for CrushMode tag events on the local player ─────────────────
    // This mirrors the pattern used by AkdShadowPortal — tag-event registration
    // means zero per-frame polling and perfectly synchronised state transitions.
    AkdMyPlayer* Player = Cast<AkdMyPlayer>(
        UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

    if (!Player)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("AkdShadow2DPlane [%s]: Local player not found at BeginPlay. "
                "Place this actor in a level that has a valid AkdMyPlayer pawn."),
            *GetName());
        return;
    }

    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    if (!ASC) return;

    ASC->RegisterGameplayTagEvent(
        FkdGameplayTags::Get().State_CrushMode,
        EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &AkdShadow2DPlane::OnCrushModeTagChanged);

    // Sync immediately if the level starts with CrushMode already active
    if (ASC->HasMatchingGameplayTag(FkdGameplayTags::Get().State_CrushMode))
    {
        bCrushActive = true;
        FadeAlpha = 1.f;
        ShadowPlaneMesh->SetVisibility(true);
        PushMaterialParameters(1.f);
        SetVolumeOverlapEventsActive(true);
        SetStencilOnOverlappingActors(true);
    }
}

void AkdShadow2DPlane::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    if (FMath::IsNearlyZero(FadeDirection)) return;

    // Advance normalised alpha toward 0 or 1
    FadeAlpha = FMath::Clamp(
        FadeAlpha + FadeDirection * (DeltaTime / FadeDuration),
        0.f, 1.f);

    // Designer-authored curve remaps linear time to a custom easing shape.
    // Null → linear, which is a safe non-distracting default.
    const float Evaluated = FadeAlphaCurve
        ? FadeAlphaCurve->GetFloatValue(FadeAlpha)
        : FadeAlpha;

    PushMaterialParameters(Evaluated);

    // ── Fade-in complete ───────────────────────────────────────────────────────
    if (FadeDirection > 0.f && FadeAlpha >= 1.f)
    {
        FadeDirection = 0.f;
        SetActorTickEnabled(false);
        // Apply stencil now that the plane is fully visible — stencil during the
        // fade would cause the PP effect to pop in, which looks worse than the blend.
        SetStencilOnOverlappingActors(true);
        BP_OnShadowFadeInComplete();
    }
    // ── Fade-out complete ──────────────────────────────────────────────────────
    else if (FadeDirection < 0.f && FadeAlpha <= 0.f)
    {
        FadeDirection = 0.f;
        ShadowPlaneMesh->SetVisibility(false);
        SetActorTickEnabled(false);
        // Clear stencil and unbind overlap delegates — no wasted callbacks in 3D mode
        SetStencilOnOverlappingActors(false);
        SetVolumeOverlapEventsActive(false);
        BP_OnShadowFadeOutComplete();
    }
}

void AkdShadow2DPlane::OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    const bool bEntering = NewCount > 0;

    if (bEntering && !bCrushActive)
    {
        bCrushActive = true;
        FadeDirection = 1.f;
        ShadowPlaneMesh->SetVisibility(true);
        SetVolumeOverlapEventsActive(true);
        SetActorTickEnabled(true);

#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log, TEXT("Shadow2DPlane [%s]: Fading IN"), *GetName());
#endif
    }
    else if (!bEntering && bCrushActive)
    {
        bCrushActive = false;
        FadeDirection = -1.f;
        SetActorTickEnabled(true);
        // Stencil and overlap delegates are cleared when fade-out completes in Tick.

#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Log, TEXT("Shadow2DPlane [%s]: Fading OUT"), *GetName());
#endif
    }
}

void AkdShadow2DPlane::OnActorEnterShadowVolume(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Only stamp stencil while CrushMode is fully active, not during fade-in,
    // so the PP effect doesn't pop on before the plane is visible.
    if (bCrushActive && FadeAlpha >= 1.f && IsValid(OtherActor))
    {
        SetActorShadowStencil(OtherActor, true);
    }
}

void AkdShadow2DPlane::OnActorExitShadowVolume(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    // Always clear stencil on exit regardless of CrushMode state so no actor
    // is left with a stale stencil value after leaving the shadow zone.
    if (IsValid(OtherActor))
    {
        SetActorShadowStencil(OtherActor, false);
    }
}

void AkdShadow2DPlane::SetVolumeOverlapEventsActive(bool bActive)
{
    if (bActive && !bOverlapBound)
    {
        ShadowVolumeBox->OnComponentBeginOverlap.AddDynamic(
            this, &AkdShadow2DPlane::OnActorEnterShadowVolume);
        ShadowVolumeBox->OnComponentEndOverlap.AddDynamic(
            this, &AkdShadow2DPlane::OnActorExitShadowVolume);
        bOverlapBound = true;
    }
    else if (!bActive && bOverlapBound)
    {
        ShadowVolumeBox->OnComponentBeginOverlap.RemoveDynamic(
            this, &AkdShadow2DPlane::OnActorEnterShadowVolume);
        ShadowVolumeBox->OnComponentEndOverlap.RemoveDynamic(
            this, &AkdShadow2DPlane::OnActorExitShadowVolume);
        bOverlapBound = false;
    }
}

void AkdShadow2DPlane::SetStencilOnOverlappingActors(bool bEnable)
{
    TArray<AActor*> OverlappingActors;
    ShadowVolumeBox->GetOverlappingActors(OverlappingActors);

    for (AActor* Actor : OverlappingActors)
    {
        if (IsValid(Actor))
        {
            SetActorShadowStencil(Actor, bEnable);
        }
    }
}

void AkdShadow2DPlane::SetActorShadowStencil(AActor* TargetActor, bool bEnable) const
{
    if (!IsValid(TargetActor)) return;

    TArray<UPrimitiveComponent*> Primitives;
    TargetActor->GetComponents<UPrimitiveComponent>(Primitives);

    for (UPrimitiveComponent* Prim : Primitives)
    {
        if (!Prim) continue;

        // EnableRenderCustomDepth must always be set before the stencil value,
        // otherwise the stencil write is silently skipped by the renderer.
        Prim->SetRenderCustomDepth(bEnable);
        if (bEnable)
        {
            Prim->SetCustomDepthStencilValue(ShadowStencilValue);
        }
    }
}

void AkdShadow2DPlane::PushMaterialParameters(float EvaluatedAlpha) const
{
    if (!DynMaterial) return;

    // ShadowOpacity drives how strongly the multiply takes effect.
    // At PeakOpacity the scene is multiplied by ShadowColor — values < 1.0
    // darken it; the hue of ShadowColor imparts the cool blue shadow tint.
    DynMaterial->SetScalarParameterValue(TEXT("ShadowOpacity"), EvaluatedAlpha * PeakOpacity);
    DynMaterial->SetVectorParameterValue(TEXT("ShadowColor"), ShadowColor);
}
