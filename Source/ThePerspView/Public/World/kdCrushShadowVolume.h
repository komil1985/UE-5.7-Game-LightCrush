// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "kdCrushShadowVolume.generated.h"


/** Which crush axis a shadow volume responds to. */
UENUM(BlueprintType)
enum class EkdCrushAxisFilter : uint8
{
    AnyAxis   UMETA(DisplayName = "Any Axis"),
    XAxisOnly UMETA(DisplayName = "X Only (North / South)"),
    YAxisOnly UMETA(DisplayName = "Y Only (East / West)"),
};


class UStaticMeshComponent;
class UAbilitySystemComponent;
class AkdMyPlayer;
UCLASS()
class THEPERSPVIEW_API AkdCrushShadowVolume : public AActor
{
    GENERATED_BODY()

public:
    AkdCrushShadowVolume();

    // ── Shape (world cm) ──────────────────────────────────────────────────────
    /** Horizontal screen width of the shadow column. */
    UPROPERTY(EditAnywhere, Category = "Crush Shadow|Shape", meta = (ClampMin = "1.0"))
    float ZoneWidthY = 600.f;

    /** Floor-to-ceiling height. */
    UPROPERTY(EditAnywhere, Category = "Crush Shadow|Shape", meta = (ClampMin = "1.0"))
    float ZoneHeightZ = 800.f;

    /** Depth thickness along the camera axis. Only needs to span the play depth. */
    UPROPERTY(EditAnywhere, Category = "Crush Shadow|Shape", meta = (ClampMin = "1.0"))
    float ZoneDepthX = 1200.f;

    // ── Render ────────────────────────────────────────────────────────────────
    /** Stencil tag the Crush PP material tests. Project convention: 2 = darken. */
    UPROPERTY(EditAnywhere, Category = "Crush Shadow|Render", meta = (ClampMin = "1", ClampMax = "255"))
    int32 ShadowStencilValue = 2;

    // ── Depth tracking ────────────────────────────────────────────────────────
    /** Snap depth (X) to the player on Crush entry — aligns with the geometry
     *  UkdGeometryTransitionComponent slides into place. */
    UPROPERTY(EditAnywhere, Category = "Crush Shadow|Depth")
    bool bTrackPlayerDepth = true;

    /** Gap (cm) placed BEHIND the player so the player's stencil-1 wins the depth
     *  test and stays lit inside the shadow. */
    UPROPERTY(EditAnywhere, Category = "Crush Shadow|Depth", meta = (ClampMin = "0.0"))
    float DepthBehindPlayer = 50.f;

    /** Restrict this volume to one collapse axis. Default Any keeps it active on
    *  every crush; set Y-only to make it appear solely on East/West crushes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crush Shadow Volume")
    EkdCrushAxisFilter ActivationAxisFilter = EkdCrushAxisFilter::AnyAxis;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Crush Shadow")
    TObjectPtr<UStaticMeshComponent> VolumeMesh;

    UPROPERTY()
    TObjectPtr<AkdMyPlayer> CachedPlayer;

    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> CachedASC;

    FDelegateHandle CrushTagHandle;
    FVector OriginalPlacedLocation = FVector::ZeroVector;

    void ApplyZoneScale(bool bCollapsesY);
    void SetVolumeActive(bool bActive);

    void OnCrushModeTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
};