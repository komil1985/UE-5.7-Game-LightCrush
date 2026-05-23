// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "kdHUDColorLibrary.generated.h"

class UkdColorTheme;

/**
 * Thin BlueprintPure wrappers around UkdColorTheme.
 *
 * UMG widgets call these instead of touching the data asset fields directly
 * so the rename of any palette field requires only a one-line change here.
 */
UCLASS()
class THEPERSPVIEW_API UkdHUDColorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Stamina bar fill colour.
     * Full stamina → PaleIon (pale cyan-violet).
     * Below 30 %    → lerps toward ExhaustRed.
     */
    UFUNCTION(BlueprintPure, Category = "HUD|Colors",
        meta = (DisplayName = "Get Stamina Bar Color"))
    static FLinearColor GetStaminaBarColor(const UkdColorTheme* Theme, float FillFraction);

    /**
     * Health bar fill colour.
     * Full health → Sunmark (amber gold).
     * Below 30 %   → lerps toward EmberTrace (warm coral).
     */
    UFUNCTION(BlueprintPure, Category = "HUD|Colors",
        meta = (DisplayName = "Get Health Bar Color"))
    static FLinearColor GetHealthBarColor(const UkdColorTheme* Theme, float FillFraction);

    /**
     * World-appropriate text colour.
     * Light world → Inkbrown (warm brown).
     * Crush world → Lumen    (matches outline trace).
     */
    UFUNCTION(BlueprintPure, Category = "HUD|Colors",
        meta = (DisplayName = "Get World Text Color"))
    static FLinearColor GetWorldTextColor(const UkdColorTheme* Theme, bool bInCrushMode);

    /**
     * HUD panel background colour.
     * Light world → Atmosphere   (warm cream, opaque).
     * Crush world → IndigoField  (semi-transparent so traces show through).
     */
    UFUNCTION(BlueprintPure, Category = "HUD|Colors",
        meta = (DisplayName = "Get HUD Panel Color"))
    static FLinearColor GetHUDPanelColor(const UkdColorTheme* Theme, bool bInCrushMode);

    /**
     * A 0–1 sine pulse for the State_Exhausted UI flash.
     * Wire TimeSeconds to "Get Game Time In Seconds" and the output to a
     * widget colour-alpha or render-opacity binding.
     *
     * @param PulseRate  Cycles per second.  Default 3 = fast danger pulse.
     */
    UFUNCTION(BlueprintPure, Category = "HUD|Colors",
        meta = (DisplayName = "Get Exhausted Pulse Alpha"))
    static float GetExhaustedPulseAlpha(float TimeSeconds, float PulseRate = 3.f);

    /**
     * Screen-overlay tint for the crush-toggle transition.
     *
     * Heliograph behaviour:
     *   Mid-blend the screen briefly washes Lumen (warm white-gold) — like
     *   the moment of a camera shutter opening or a sun-print being exposed.
     *   Peaks at 25 % opacity at Alpha = 0.5 and fades to zero at both ends.
     *
     * BlendAlpha comes from UkdWorldColorDriver::GetBlendAlpha().
     * Wire to a full-screen UMG image widget covering the viewport.
     */
    UFUNCTION(BlueprintPure, Category = "HUD|Colors",
        meta = (DisplayName = "Get Transition Flash Color"))
    static FLinearColor GetTransitionFlashColor(const UkdColorTheme* Theme, float BlendAlpha);

    /**
     * Shadow-portal glow colour that reacts to stamina.
     * Full stamina → PaleIon  (inviting).
     * Empty stamina → Steelgrey (greyed out — "not usable right now").
     */
    UFUNCTION(BlueprintPure, Category = "HUD|Colors",
        meta = (DisplayName = "Get Portal Glow Color"))
    static FLinearColor GetPortalGlowColor(const UkdColorTheme* Theme, float StaminaFraction);

    /**
     * Outline glow colour for world geometry / pickups.
     * Returns Lumen for normal objects and EmberTrace for hazards — wire to
     * material edge-glow params or world-space outline widget tints.
     */
    UFUNCTION(BlueprintPure, Category = "HUD|Colors",
        meta = (DisplayName = "Get Outline Glow Color"))
    static FLinearColor GetOutlineGlowColor(const UkdColorTheme* Theme, bool bIsHazard);

private:
    /** Returns false and logs a warning if Theme is null. */
    static bool ValidateTheme(const UkdColorTheme* Theme, const TCHAR* CallerName);
};
