// Copyright ASKD Games

#include "UI/ColorLibrary/kdHUDColorRamp.h"


FLinearColor FkdBarColorRamp::Evaluate(float Percent01) const
{
	if (Stops.Num() == 0) { return FLinearColor::White; }
	if (Stops.Num() == 1) { return Stops[0].Color; }

	const float P = FMath::Clamp(Percent01, 0.f, 1.f);

	// Ends (assumes ascending Threshold order).
	if (P <= Stops[0].Threshold) { return Stops[0].Color; }
	if (P >= Stops.Last().Threshold) { return Stops.Last().Color; }

	for (int32 i = 0; i < Stops.Num() - 1; ++i)
	{
		const FkdBarColorStop& A = Stops[i];
		const FkdBarColorStop& B = Stops[i + 1];

		if (P >= A.Threshold && P <= B.Threshold)
		{
			const float Span = B.Threshold - A.Threshold;
			const float Alpha = (Span > KINDA_SMALL_NUMBER) ? (P - A.Threshold) / Span : 0.f;

			return bUseHSVBlend
				? FLinearColor::LerpUsingHSV(A.Color, B.Color, Alpha)
				: FMath::Lerp(A.Color, B.Color, Alpha);
		}
	}

	return Stops.Last().Color;
}
