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
#include "Engine/Texture2D.h"
#include "RHI.h"
#include "Crush/kdCrushStateComponent.h"

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
    // X — stays thin (1 × 100 cm = 1 m depth)
    // Y → world Y covers full level width
    // Z → world Z covers floor-to-ceiling
    ShadowPlaneMesh->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
    ShadowPlaneMesh->SetRelativeScale3D(FVector(PlaneHeight / 100.f, PlaneWidth / 100.f, 1.0f));                    

    // ── Size the stencil detection volume ─────────────────────────────────────
    ShadowVolumeBox->SetBoxExtent(FVector(VolumeThickness * 0.5f, PlaneWidth * 0.5f, PlaneHeight * 0.5f));

    ApplyGeometryToComponents();

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
    AkdMyPlayer* Player = Cast<AkdMyPlayer>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

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

#if !UE_BUILD_SHIPPING
    {
        const FVector MeshBounds = ShadowPlaneMesh->Bounds.BoxExtent * 2.f;  // full extents
        const FVector BoxBounds = ShadowVolumeBox->GetScaledBoxExtent() * 2.f;
        // Compare world Y and Z; X is intentionally different (plane is flat, box is thick).
        ensureMsgf(FMath::IsNearlyEqual(MeshBounds.Y, BoxBounds.Y, 1.f) &&
            FMath::IsNearlyEqual(MeshBounds.Z, BoxBounds.Z, 1.f),
            TEXT("Shadow2DPlane [%s]: mesh/volume mismatch — "
                "mesh=(%.0f, %.0f), box=(%.0f, %.0f)"),
            *GetName(), MeshBounds.Y, MeshBounds.Z, BoxBounds.Y, BoxBounds.Z);
    }
#endif
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

void AkdShadow2DPlane::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyGeometryToComponents();
}

void AkdShadow2DPlane::EnsureLightMaskTexture()
{
    if (LightMaskTex && LightMaskTex->GetSizeX() == LightMaskResolution) return;

    LightMaskTex = UTexture2D::CreateTransient(LightMaskResolution, LightMaskResolution, PF_G8);

    LightMaskTex->Filter = TF_Bilinear;     // smooth between cells
    LightMaskTex->AddressX = TA_Clamp;
    LightMaskTex->AddressY = TA_Clamp;
    LightMaskTex->SRGB = false;           // linear data, not colour
    LightMaskTex->CompressionSettings = TC_Grayscale;
    LightMaskTex->NeverStream = true;
    LightMaskTex->UpdateResource();
}

void AkdShadow2DPlane::UploadLightMaskPixels(const TArray<uint8>& Pixels)
{
    if (!LightMaskTex) return;

    const int32 N = LightMaskResolution;

    // FUpdateTextureRegion2D is consumed on the render thread — heap-allocate
    // and delete in the cleanup lambda so we don't free it before the GPU reads.
    FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, N, N);

    // We copy the pixel buffer because Pixels is a stack-local TArray.
    uint8* PixelCopy = static_cast<uint8*>(FMemory::Malloc(Pixels.Num()));
    FMemory::Memcpy(PixelCopy, Pixels.GetData(), Pixels.Num());

    LightMaskTex->UpdateTextureRegions(
        /*MipIndex*/ 0,
        /*NumRegions*/ 1, Region,
        /*SrcPitch*/ N,
        /*SrcBpp*/ 1,
        PixelCopy,
        /*DataCleanupFunc*/ [](uint8* Data, const FUpdateTextureRegion2D* R)
        {
            FMemory::Free(Data);
            delete R;
        });
}

void AkdShadow2DPlane::RebakeLightMask()
{
    UWorld* World = GetWorld();
    if (!World) return;

    AkdMyPlayer* Player = Cast<AkdMyPlayer>(UGameplayStatics::GetPlayerPawn(World, 0));
    if (!Player) return;

    UkdCrushStateComponent* CrushState = Player->FindComponentByClass<UkdCrushStateComponent>();
    if (!CrushState) return;

    const TArray<FkdRegisteredLight>& Lights = CrushState->GetRegisteredLights();

    EnsureLightMaskTexture();

    const int32 N = LightMaskResolution;
    TArray<uint8> Pixels;
    Pixels.SetNumUninitialized(N * N);

    // Plane footprint in world space.  Actor pivot is the plane centre;
    // mesh spans PlaneWidth on Y and PlaneHeight on Z at the actor's X.
    const FVector ActorLoc = GetActorLocation();
    const float   YMin = ActorLoc.Y - PlaneWidth * 0.5f;
    const float   ZMin = ActorLoc.Z - PlaneHeight * 0.5f;
    const float   PlaneX = ActorLoc.X;

    // Empty light set → entire plane reads as shadow.  Fill black and bail
    // before the per-texel loop so we don't pay for N² no-op raycasts.
    if (Lights.IsEmpty())
    {
        FMemory::Memzero(Pixels.GetData(), Pixels.Num());
        UploadLightMaskPixels(Pixels);
        PushLightMaskMaterialParameters();
        return;
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(ShadowMaskBake), /*bTraceComplex*/ false);
    Params.AddIgnoredActor(this);  // never let the plane occlude its own bake

    for (int32 v = 0; v < N; ++v)
    {
        // Image V grows downward; world Z grows upward.  Flip for upright reads.
        const float ZWorld = ZMin + ((N - 1 - v) + 0.5f) / static_cast<float>(N) * PlaneHeight;

        for (int32 u = 0; u < N; ++u)
        {
            const float YWorld = YMin + (u + 0.5f) / static_cast<float>(N) * PlaneWidth;
            const FVector SamplePoint(PlaneX, YWorld, ZWorld);

            int32 Relevant = 0;
            int32 Reached = 0;

            for (const FkdRegisteredLight& Light : Lights)
            {
                const FVector LightDir = CrushState->GetLightDirectionForPlayer(Light, SamplePoint);

                // Same convention as IsStandingInShadow — zero-vector means the
                // light doesn't influence this point (out of range / out of cone).
                if (LightDir.IsNearlyZero()) continue;
                ++Relevant;

                const FVector TraceEnd = (Light.Type == EkdLightSourceType::Directional)
                    ? SamplePoint + LightDir * 10000.f
                    : Light.LightActor->GetActorLocation();

                FHitResult Hit;
                const bool bBlocked = World->LineTraceSingleByChannel(
                    Hit, SamplePoint, TraceEnd, ECC_Visibility, Params);

                if (!bBlocked) ++Reached;
            }

            // 0 = full shadow, 255 = fully lit.  No relevant lights → shadow.
            const float Lit = (Relevant > 0)
                ? static_cast<float>(Reached) / static_cast<float>(Relevant)
                : 0.f;

            Pixels[v * N + u] = static_cast<uint8>(FMath::Clamp(Lit * 255.f, 0.f, 255.f));
        }
    }

    UploadLightMaskPixels(Pixels);
    PushLightMaskMaterialParameters();
}

void AkdShadow2DPlane::PushLightMaskMaterialParameters() const
{
    if (!DynMaterial || !LightMaskTex) return;

    DynMaterial->SetTextureParameterValue(LightMaskParamName, LightMaskTex);
    DynMaterial->SetScalarParameterValue(ShadowDarknessParamName, ShadowDarkness);
    DynMaterial->SetScalarParameterValue(LightDarkeningParamName, LightDarkening);

    const FVector ActorLoc = GetActorLocation();

    DynMaterial->SetVectorParameterValue(PlaneOriginParamName,
        FLinearColor(ActorLoc.Y - PlaneWidth * 0.5f,
            ActorLoc.Z - PlaneHeight * 0.5f, 0.f, 0.f));
    
    DynMaterial->SetVectorParameterValue(PlaneSizeParamName,
        FLinearColor(PlaneWidth, PlaneHeight, 0.f, 0.f));

    DynMaterial->SetScalarParameterValue(EdgeFadeWidthParamName, EdgeFadeWidth);
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

        RebakeLightMask();

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

void AkdShadow2DPlane::ApplyGeometryToComponents()
{
    if (!ShadowVolumeBox || !ShadowPlaneMesh) return;

    // ── Volume box ────────────────────────────────────────────────────────────
    // Half-extents on world axes.  bUpdateOverlaps=false because we don't want
    // to fire begin/end overlap deltas during construction.
    ShadowVolumeBox->SetBoxExtent(
        FVector(VolumeThickness * 0.5f, PlaneWidth * 0.5f, PlaneHeight * 0.5f),
        /*bUpdateOverlaps*/ false);

    // ── Plane mesh ────────────────────────────────────────────────────────────
    // The engine Plane is 100 × 100 cm in its local XY.  After Pitch=90,
    // local X becomes world Z, local Y stays world Y.  Scale accordingly so
    // the visible mesh matches the box footprint exactly.
    ShadowPlaneMesh->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
    ShadowPlaneMesh->SetRelativeScale3D(
        FVector(PlaneHeight / 100.f,    // local X → world Z
            PlaneWidth / 100.f,         // local Y → world Y
            1.f));                      // local Z → world -X (flat)
}
