// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "kdHUDColorLibrary.generated.h"



class UkdColorTheme;
UCLASS()
class THEPERSPVIEW_API UkdHUDColorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	

    /**
    * Stamina bar fill colour.
    * Full stamina → Spectre (purple).
    * Below 30 % → lerps toward ExhaustRed.
    *
    * @param FillFraction  ShadowStamina / MaxShadowStamina  (0–1)
    */
    UFUNCTION(BlueprintPure, Category = "HUD|Colors",
        meta = (DisplayName = "Get Stamina Bar Color"))
    static FLinearColor GetStaminaBarColor(const UkdColorTheme* Theme, float FillFraction);

    /**
     * Health bar fill colour.
     * Full health → Sunstone (gold).
     * Below 30 % → lerps toward Ember (orange-red).
     *
     * @param FillFraction  LightHealth / MaxLightHealth  (0–1)
     */
    UFUNCTION(BlueprintPure, Category = "HUD|Colors",
        meta = (DisplayName = "Get Health Bar Color"))
    static FLinearColor GetHealthBarColor(const UkdColorTheme* Theme, float FillFraction);

    /**
     * World-appropriate text colour.
     * Light world → DuskOak (warm brown).
     * Crush mode  → GhostLilac (pale lavender).
     */
    UFUNCTION(BlueprintPure, Category = "HUD|Colors",
        meta = (DisplayName = "Get World Text Color"))
    static FLinearColor GetWorldTextColor(const UkdColorTheme* Theme, bool bInCrushMode);

    /**
     * HUD panel background colour.
     * Light world → Parchment (warm off-white, alpha 1.0).
     * Crush mode  → Void      (near-black indigo, alpha 0.85).
     */
    UFUNCTION(BlueprintPure, Category = "HUD|Colors",
        meta = (DisplayName = "Get HUD Panel Color"))
    static FLinearColor GetHUDPanelColor(const UkdColorTheme* Theme, bool bInCrushMode);

    /**
     * A 0–1 sine-wave pulse for the State_Exhausted UI animation.
     * Wire TimeSeconds to the "Get Game Time In Seconds" node.
     * Wire the output to a render-opacity or colour-alpha binding.
     *
     * @param PulseRate  Cycles per second.  Default 3 = fast danger pulse.
     */
    UFUNCTION(BlueprintPure, Category = "HUD|Colors",
        meta = (DisplayName = "Get Exhausted Pulse Alpha"))
    static float GetExhaustedPulseAlpha(float TimeSeconds, float PulseRate = 3.f);

    /**
     * Screen-edge overlay tint for the crush toggle transition.
     * Peaks at 35 % CrushTeal opacity at mid-blend, fades to transparent at
     * the start and end — a smooth in/out flash that confirms the mode change.
     *
     * BlendAlpha comes from UkdWorldColorDriver::GetBlendAlpha().
     * Wire this to a full-screen image widget that covers the viewport.
     */
    UFUNCTION(BlueprintPure, Category = "HUD|Colors",
        meta = (DisplayName = "Get Transition Flash Color"))
    static FLinearColor GetTransitionFlashColor(const UkdColorTheme* Theme, float BlendAlpha);

    /**
     * Shadow portal glow colour that reacts to stamina.
     * Full stamina → Spectre (full purple glow).
     * Empty stamina → Slate  (greyed out — portal hint is disabled).
     *
     * Use on portal UI indicators or the portal mesh's emissive intensity.
     */
    UFUNCTION(BlueprintPure, Category = "HUD|Colors",
        meta = (DisplayName = "Get Portal Glow Color"))
    static FLinearColor GetPortalGlowColor(const UkdColorTheme* Theme, float StaminaFraction);

private:
    /** Returns false and logs a warning if Theme is null. */
    static bool ValidateTheme(const UkdColorTheme* Theme, const TCHAR* CallerName);
};
