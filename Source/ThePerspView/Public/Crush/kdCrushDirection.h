// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "kdCrushDirection.generated.h"


/**
 * EkdCrushDirection
 *
 * The four discrete crush axes, derived from the camera/control yaw at the
 * moment the player presses the crush input.  Stored on the transition
 * component so every downstream system (plane constraint, geometry morph,
 * spring-arm placement) can read the same resolved value.
 *
 * Yaw convention (Unreal standard):
 *   0°   → facing +X world  (North / front of your default level layout)
 *   90°  → facing +Y world  (East)
 *   180° → facing -X world  (South)
 *   270° → facing -Y world  (West)
 */
UENUM(BlueprintType)
enum class EkdCrushDirection : uint8
{
    /** Camera yaw was not near any cardinal — crush is blocked. */
    None  UMETA(DisplayName = "Unaligned"),

    /** Camera faces ~North (yaw ≈ 0° / 360°). Collapses along X-axis. */
    PosX  UMETA(DisplayName = "North (+X)"),

    /** Camera faces ~South (yaw ≈ 180°). Collapses along X-axis (opposite view side). */
    NegX  UMETA(DisplayName = "South (-X)"),

    /** Camera faces ~East (yaw ≈ 90°). Collapses along Y-axis. */
    PosY  UMETA(DisplayName = "East (+Y)"),

    /** Camera faces ~West (yaw ≈ 270°). Collapses along Y-axis (opposite view side). */
    NegY  UMETA(DisplayName = "West (-Y)"),
};
