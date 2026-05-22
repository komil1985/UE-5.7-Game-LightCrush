// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "kdShadow2DPlane.generated.h"


class UStaticMeshComponent;
class UBoxComponent;
class UMaterialIstanceDynamic;
class UCurveFloat;
UCLASS()
class THEPERSPVIEW_API AkdShadow2DPlane : public AActor
{
	GENERATED_BODY()
	
public:	
	AkdShadow2DPlane();

	UPROPERTY(EditAnywhere, Category = "Shadow | Geometry", meta = (ClampMin = "100.0"))
	float PlaneWidth = 3000.0f;
	
	UPROPERTY(EditAnywhere, Category = "Shadow | Geometry", meta = (ClampMin = "100.0"))
	float PlaneHeight = 5000.0f;
	
	UPROPERTY(EditAnywhere, Category = "Shadow | Geometry", meta = (ClampMin = "1.0"))
	float VolumeThickness = 80.0f;

	UPROPERTY(EditAnywhere, Category = "Shadow | Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PeakOpacity = 0.55f;

	UPROPERTY(EditDefaultsOnly, Category = "Shadow|Material")
	FLinearColor ShadowColor = FLinearColor(0.18f, 0.20f, 0.35f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Shadow|Material", meta = (ClampMin = "0.05"))
	float FadeDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Shadow|Material")
	TObjectPtr<UCurveFloat> FadeAlphaCurve;

	UPROPERTY(EditDefaultsOnly, Category = "Shadow|Stencil", meta = (ClampMin = "1", ClampMax = "255"))
	int32 ShadowStencilValue = 2;

	UFUNCTION(BlueprintImplementableEvent, Category = "Shadow")
	void BP_OnShadowFadeInComplete();

	UFUNCTION(BlueprintImplementableEvent, Category = "Shadow")
	void BP_OnShadowFadeOutComplete();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> ShadowPlaneMesh;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> ShadowVolumeBox;

    // ── Runtime State ─────────────────────────────────────────────────────────

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> DynMaterial;

    float FadeAlpha = 0.f;   // Normalised fade progress [0,1]
    float FadeDirection = 0.f;   // +1 fading in  |  -1 fading out  |  0 idle
    bool  bCrushActive = false; // True while State_CrushMode is present
    bool  bOverlapBound = false; // Guard: overlap delegates bound exactly once

    UFUNCTION()
    void OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount);

    UFUNCTION()
    void OnActorEnterShadowVolume(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnActorExitShadowVolume(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    // ── Helpers ───────────────────────────────────────────────────────────────
    void SetVolumeOverlapEventsActive(bool bActive);
    void SetStencilOnOverlappingActors(bool bEnable);
    void SetActorShadowStencil(AActor* TargetActor, bool bEnable) const;
    void PushMaterialParameters(float EvaluatedAlpha) const;
};
