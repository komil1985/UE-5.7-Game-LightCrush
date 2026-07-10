// Copyright ASKD Games

#include "World/kdSunprintAnnotation.h"
#include "GameplayTags/kdGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"


AkdSunprintAnnotation::AkdSunprintAnnotation()
{
    // Tick is opt-in: enabled only while a fade is in flight, then self-disabled.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    AnnotationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AnnotationMesh"));
    SetRootComponent(AnnotationMesh);

    AnnotationMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AnnotationMesh->SetCastShadow(false);
    AnnotationMesh->bCastHiddenShadow = false;      // both are required
    AnnotationMesh->SetRenderCustomDepth(true);
    AnnotationMesh->SetCustomDepthStencilValue(1);  // neon-preserve
    AnnotationMesh->SetMobility(EComponentMobility::Movable);
}

void AkdSunprintAnnotation::BeginPlay()
{
    Super::BeginPlay();

    DynMat = AnnotationMesh->CreateDynamicMaterialInstance(0);
    CurrentOpacity = TargetOpacity = 0.f;
    if (DynMat) DynMat->SetScalarParameterValue(OpacityParamName, 0.f);
    AnnotationMesh->SetHiddenInGame(true);

    // The pawn may not be possessed yet on the first frame. Defer one tick —
    // the same guard the audio pipeline needed for PostLoadMapWithWorld.
    GetWorld()->GetTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateWeakLambda(this, [this]() { BindToPlayerASC(); }));
}

void AkdSunprintAnnotation::BindToPlayerASC()
{
    APawn* P = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(P);
    if (!ASC) return;

    BoundASC = ASC;
    const FGameplayTag& CrushTag = FkdGameplayTags::Get().State_CrushMode;

    CrushTagHandle = ASC->RegisterGameplayTagEvent(CrushTag, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &AkdSunprintAnnotation::OnCrushModeTagChanged);

    // Seed from current state so an annotation placed inside an already-crushed
    // world is correct on frame one.
    const bool bInCrush = ASC->HasMatchingGameplayTag(CrushTag);
    SetTargetVisible(bVisibleInCrushOnly ? bInCrush : !bInCrush);
    CurrentOpacity = TargetOpacity;                       // no fade on seed
    if (DynMat) DynMat->SetScalarParameterValue(OpacityParamName, CurrentOpacity);
    AnnotationMesh->SetHiddenInGame(CurrentOpacity <= KINDA_SMALL_NUMBER);
}

void AkdSunprintAnnotation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (BoundASC.IsValid() && CrushTagHandle.IsValid())
    {
        BoundASC->UnregisterGameplayTagEvent(
            CrushTagHandle, FkdGameplayTags::Get().State_CrushMode, EGameplayTagEventType::NewOrRemoved);
    }
    CrushTagHandle.Reset();
    BoundASC = nullptr;

    Super::EndPlay(EndPlayReason);
}

void AkdSunprintAnnotation::OnCrushModeTagChanged(const FGameplayTag, int32 NewCount)
{
    const bool bInCrush = (NewCount > 0);
    SetTargetVisible(bVisibleInCrushOnly ? bInCrush : !bInCrush);
}

void AkdSunprintAnnotation::SetTargetVisible(bool bVisible)
{
    TargetOpacity = bVisible ? 1.f : 0.f;

    if (bVisible) AnnotationMesh->SetHiddenInGame(false);   // reveal before fading in
    if (!FMath::IsNearlyEqual(CurrentOpacity, TargetOpacity))
    {
        SetActorTickEnabled(true);
    }
}

void AkdSunprintAnnotation::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    const float Step = DeltaSeconds / FMath::Max(FadeSeconds, KINDA_SMALL_NUMBER);
    CurrentOpacity = FMath::FInterpConstantTo(CurrentOpacity, TargetOpacity, 1.f, Step);

    if (DynMat) DynMat->SetScalarParameterValue(OpacityParamName, CurrentOpacity);

    if (FMath::IsNearlyEqual(CurrentOpacity, TargetOpacity, 0.001f))
    {
        CurrentOpacity = TargetOpacity;
        AnnotationMesh->SetHiddenInGame(CurrentOpacity <= KINDA_SMALL_NUMBER);
        SetActorTickEnabled(false);   // back to zero cost
    }
}