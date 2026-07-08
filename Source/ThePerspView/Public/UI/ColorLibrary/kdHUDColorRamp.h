// Copyright ASKD Games
#pragma once

#include "CoreMinimal.h"
#include "kdHUDColorRamp.generated.h"

/**
 * Canonical Heliograph palette.
 *
 * Hex swatches are authored in sRGB, so we convert through FromSRGBColor to get the
 * correct LINEAR value UMG expects. Passing a raw FColor->FLinearColor (which skips the
 * gamma conversion) would wash the colors out — always go through FromSRGBColor here.
 */
namespace kdHeliograph
{
	FORCEINLINE FLinearColor SRGB(uint8 R, uint8 G, uint8 B)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B, 255));
	}

	inline const FLinearColor IndigoField = SRGB(0x0E, 0x15, 0x38);
	inline const FLinearColor Lumen = SRGB(0xF5, 0xE7, 0xB8);
	inline const FLinearColor SolarWhite = SRGB(0xFC, 0xFA, 0xF0);
	inline const FLinearColor PaleIon = SRGB(0xA8, 0xC0, 0xFF);
	inline const FLinearColor Atmosphere = SRGB(0xE8, 0xDD, 0xC8);
	inline const FLinearColor Sunmark = SRGB(0xE8, 0xB8, 0x4B);
	inline const FLinearColor EmberTrace = SRGB(0xFF, 0x6B, 0x47);
	inline const FLinearColor ExhaustRed = SRGB(0xE8, 0x40, 0x40);
	inline const FLinearColor GoldLeaf = SRGB(0xF0, 0xC0, 0x60);
	inline const FLinearColor Steelgrey = SRGB(0x4A, 0x4F, 0x5E);
}

/** One stop on a bar's color ramp: "at THIS fill fraction, the bar is THIS color". */
USTRUCT(BlueprintType)
struct FkdBarColorStop
{
	GENERATED_BODY()

	/** Fill fraction (0..1) at which this color applies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "kd|HUD", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Threshold = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "kd|HUD")
	FLinearColor Color = FLinearColor::White;

	FkdBarColorStop() = default;
	FkdBarColorStop(float InThreshold, const FLinearColor& InColor)
		: Threshold(InThreshold), Color(InColor) {
	}
};

/**
 * A percentage-driven color ramp for a HUD bar.
 *
 * Stops must be ordered ascending by Threshold (0 -> 1). Evaluate() clamps at the ends
 * and lerps between the two bracketing stops. Keep segments short (3-4 stops) and the
 * linear-RGB blend stays clean; flip bUseHSVBlend if you want hue-rotated transitions.
 */
USTRUCT(BlueprintType)
struct FkdBarColorRamp
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "kd|HUD")
	TArray<FkdBarColorStop> Stops;

	/** true = blend in HSV (nicer hue sweeps); false = linear RGB (truer to palette). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "kd|HUD")
	bool bUseHSVBlend = false;

	/** Returns the interpolated color for the given fill fraction. */
	THEPERSPVIEW_API FLinearColor Evaluate(float Percent01) const;
};
