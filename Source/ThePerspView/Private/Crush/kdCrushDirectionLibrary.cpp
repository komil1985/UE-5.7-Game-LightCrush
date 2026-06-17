// Copyright ASKD Games

#include "Crush/kdCrushDirectionLibrary.h"

// ─────────────────────────────────────────────────────────────────────────────
// ResolveCrushDirection
//
// The math has three deliberate decisions:
//
//  1. FRotator::ClampAxis winds the input into [0, 360) regardless of whether
//     the controller handed us -90, 270, or 630 — all the same yaw.
//
//  2. RoundToInt / & 3 maps any yaw to an index 0..3 (N/E/S/W) with correct
//     360→0 wrapping.  A yaw of ~350° rounds to index 4; 4 & 3 = 0 (North).
//
//  3. FMath::FindDeltaAngleDegrees gives the *signed* shortest arc between two
//     angles, handling the 0°/360° seam correctly.  Taking Abs() of that is
//     the only correct way to compute angular error across that seam.
//     (FMath::Abs(Yaw - SnappedYaw) is wrong for yaw near 0° vs 360°.)
// ─────────────────────────────────────────────────────────────────────────────

EkdCrushDirection UkdCrushDirectionLibrary::ResolveCrushDirection(
    float  ControlYawDegrees,
    float  ToleranceDegrees,
    float& OutAlignmentErrorDegrees)
{
    // Wind into [0, 360) — safe regardless of controller yaw convention.
    const float Yaw = FRotator::ClampAxis(ControlYawDegrees);

    // Index of the nearest cardinal (0=N, 1=E, 2=S, 3=W).
    // Masking with & 3 folds the 360°/0° boundary: RoundToInt(350/90)=4, 4&3=0.
    const int32 CardinalIndex = FMath::RoundToInt(Yaw / 90.f) & 3;
    const float SnappedYaw = static_cast<float>(CardinalIndex) * 90.f;

    // Correct shortest-arc error (handles 359° vs 1° correctly).
    OutAlignmentErrorDegrees = FMath::Abs(
        FMath::FindDeltaAngleDegrees(Yaw, SnappedYaw));

    // Outside tolerance → no crush, per spec.
    if (OutAlignmentErrorDegrees > ToleranceDegrees)
    {
        return EkdCrushDirection::None;
    }

    switch (CardinalIndex)
    {
    case 0:  return EkdCrushDirection::PosX;   // North → +X
    case 1:  return EkdCrushDirection::PosY;   // East  → +Y
    case 2:  return EkdCrushDirection::NegX;   // South → -X
    case 3:  return EkdCrushDirection::NegY;   // West  → -Y
    default: return EkdCrushDirection::None;   // unreachable — CardinalIndex is 0..3
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DirectionToCollapseNormal
//
// Note: PosX and NegX intentionally return the identical normal (1,0,0).
// Both directions collapse onto the YZ plane.  The sign only tells you which
// SIDE of that plane the camera approaches from — it is not part of the CMC
// plane-constraint normal.  Same logic applies to PosY / NegY → (0,1,0).
// ─────────────────────────────────────────────────────────────────────────────

FVector UkdCrushDirectionLibrary::DirectionToCollapseNormal(EkdCrushDirection Direction)
{
    switch (Direction)
    {
    case EkdCrushDirection::PosX:
    case EkdCrushDirection::NegX: return FVector(1.f, 0.f, 0.f);
    case EkdCrushDirection::PosY:
    case EkdCrushDirection::NegY: return FVector(0.f, 1.f, 0.f);
    default:                      return FVector::ZeroVector;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DirectionToViewVector
//
// The signed unit vector pointing from the camera toward the collapse plane.
// Feed this into spring-arm orientation and geometry/shadow placement so that
// a +X crush and a -X crush look and behave as mirror images of each other.
// ─────────────────────────────────────────────────────────────────────────────

FVector UkdCrushDirectionLibrary::DirectionToViewVector(EkdCrushDirection Direction)
{
    switch (Direction)
    {
    case EkdCrushDirection::PosX: return FVector(1.f, 0.f, 0.f);
    case EkdCrushDirection::NegX: return FVector(-1.f, 0.f, 0.f);
    case EkdCrushDirection::PosY: return FVector(0.f, 1.f, 0.f);
    case EkdCrushDirection::NegY: return FVector(0.f, -1.f, 0.f);
    default:                      return FVector::ZeroVector;
    }
}

FkdCrushBasis UkdCrushDirectionLibrary::MakeCrushBasis(EkdCrushDirection Direction)
{
    FkdCrushBasis Basis;

    // None falls back to the North/X default so callers never get a zero basis.
    if (Direction == EkdCrushDirection::None)
    {
        return Basis;
    }

    Basis.ViewForward = DirectionToViewVector(Direction);         // camera forward
    Basis.CollapseNormal = DirectionToCollapseNormal(Direction);  // unsigned axis

    // Screen-right = Up × Forward.  Verified per cardinal:
    //   North  fwd(1,0,0)  → right(0,1,0)   (matches your current 2D walk)
    //   East   fwd(0,1,0)  → right(-1,0,0)
    //   South  fwd(-1,0,0) → right(0,-1,0)
    //   West   fwd(0,-1,0) → right(1,0,0)
    Basis.WalkRight = FVector::CrossProduct(FVector::UpVector, Basis.ViewForward).GetSafeNormal();

    // Yaw that makes the camera face ViewForward (0/90/180/270 for N/E/S/W).
    Basis.CameraYaw = Basis.ViewForward.Rotation().Yaw;

    Basis.bCollapsesY = (Basis.CollapseNormal.Y > 0.5f);

    return Basis;
}
