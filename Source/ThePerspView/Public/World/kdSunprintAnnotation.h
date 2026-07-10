// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "kdSunprintAnnotation.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * AkdSunprintAnnotation — a drafting mark that only exists in the shadow plane.
 *
 * A chalk arrow, a dotted jump arc, a compass rose showing the collapse axis.
 * Because it is only legible in Crush Mode, the instruction is inseparable from
 * the mechanic being taught: you must use the tool to read the manual.
 *
 * RENDERING CONTRACT:
 *   • Custom Depth stencil 1 = "neon-preserve" — never desaturated by the driver.
 *   • Never casts shadows. bRenderInMainPass alone does NOT stop shadow casting;
 *     SetCastShadow(false) + bCastHiddenShadow = false are both required.
 *   • Fades via a scalar "Opacity" param, matching AkdShadowPortal's convention.
 */
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API AkdSunprintAnnotation : public AActor
{
    GENERATED_BODY()

public:
    AkdSunprintAnnotation();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> AnnotationMesh;

    /** Scalar param on the annotation material. */
    UPROPERTY(EditDefaultsOnly, Category = "Sunprint")
    FName OpacityParamName = FName("Opacity");

    UPROPERTY(EditAnywhere, Category = "Sunprint", meta = (ClampMin = "0.01"))
    float FadeSeconds = 0.5f;

    /** Invert to author marks that only exist in the 3D world. */
    UPROPERTY(EditAnywhere, Category = "Sunprint")
    bool bVisibleInCrushOnly = true;

private:
    void BindToPlayerASC();
    void OnCrushModeTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
    void SetTargetVisible(bool bVisible);

    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DynMat = nullptr;

    TWeakObjectPtr<class UAbilitySystemComponent> BoundASC;
    FDelegateHandle CrushTagHandle;

    float CurrentOpacity = 0.f;
    float TargetOpacity = 0.f;
};