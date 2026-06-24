// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "kdCrushLandingInterface.generated.h"


/** Designer rule: can a reverting (2D→3D) player re-ground on this surface? */
UENUM(BlueprintType)
enum class EkdCrushStandRule : uint8
{
    /** Valid landing surface on revert. Tinted with the normal world colour. */
    Standable    UMETA(DisplayName = "Standable After Crush"),
    /** Not a landing surface — player falls through on revert. Tinted as a hazard. */
    NonStandable UMETA(DisplayName = "Not Standable After Crush"),
};


UINTERFACE(MinimalAPI, BlueprintType)
class UkdCrushLandingInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class THEPERSPVIEW_API IkdCrushLandingInterface
{
	GENERATED_BODY()

public:

    /** True if a reverting player may stand on this surface. */
    virtual bool IsStandableAfterCrush() const = 0;

    /**
     * If WorldXY lies within this surface's RESTORED footprint, returns true and
     * writes the world-space top-surface Z. Axis-aligned floors only (project
     * convention: geometry is unrotated).
     */
    virtual bool GetRestoredStandingZ(const FVector2D& WorldXY, float& OutTopZ) const = 0;
};
