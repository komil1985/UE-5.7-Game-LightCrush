// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Crush/kdCrushDirection.h"
#include "kdCrushDirectionLibrary.generated.h"

/**
 * UkdCrushDirectionLibrary
 *
 * Stateless resolver that maps a raw control-rotation yaw to a discrete
 * EkdCrushDirection, gated by a designer-tunable tolerance.
 *
 * Single source of truth — call this from kd_CrushToggle, from the HUD
 * alignment reticle, or from any debug tooling.  No instancing required.
 */
UCLASS()
class THEPERSPVIEW_API UkdCrushDirectionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

    /**
     * ResolveCrushDirection
     *
     * Maps ControlYawDegrees to the nearest cardinal direction and returns
     * the matching EkdCrushDirection.  Returns None if the yaw error exceeds
     * ToleranceDegrees.
     *
     * @param ControlYawDegrees       Raw yaw from AController::GetControlRotation().Yaw.
     * @param ToleranceDegrees        Maximum angular deviation from a cardinal to allow crush.
     *                                Recommended range: 8°–20°.  Tune in the ability CDO.
     * @param OutAlignmentErrorDegrees Absolute angular error (degrees) from the nearest
     *                                cardinal — useful for HUD reticle feedback.
     * @return                        Resolved direction, or EkdCrushDirection::None if blocked.
     */
    UFUNCTION(BlueprintPure, Category = "kd|Crush|Direction",
        meta = (Keywords = "crush yaw cardinal direction alignment"))
    static EkdCrushDirection ResolveCrushDirection(
        float  ControlYawDegrees,
        float  ToleranceDegrees,
        float& OutAlignmentErrorDegrees);

    /**
     * DirectionToCollapseNormal
     *
     * Returns the CMC plane-constraint normal for a given crush direction.
     * PosX and NegX share the same normal (1,0,0) because they collapse to
     * the same YZ plane — only the view SIDE differs, not the play surface.
     * PosY and NegY share (0,1,0) for the same reason.
     */
    UFUNCTION(BlueprintPure, Category = "kd|Crush|Direction")
    static FVector DirectionToCollapseNormal(EkdCrushDirection Direction);

    /**
     * DirectionToViewVector
     *
     * Returns the signed unit vector representing which world side the camera
     * is coming FROM.  Feed this to spring-arm placement and geometry/shadow
     * orientation so +X-crush and -X-crush produce the correct camera side.
     */
    UFUNCTION(BlueprintPure, Category = "kd|Crush|Direction")
    static FVector DirectionToViewVector(EkdCrushDirection Direction);
};
