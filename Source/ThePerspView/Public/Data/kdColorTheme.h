// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "kdColorTheme.generated.h"

class UMaterialParameterCollection;

// ─────────────────────────────────────────────────────────────────────────────
// FkdPostProcessProfile
//
// Holds every post-process setting that differs between the Light World and the
// Shadow Plane.  UkdWorldColorDriver lerps between two of these at runtime.
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FkdPostProcessProfile
{
    GENERATED_BODY()

    /** Multiplicative scene tint.  Warm amber for light; cool indigo for shadow. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color Grading")
    FLinearColor SceneColorTint = FLinearColor(1.f, 1.f, 1.f, 1.f);

    /**
     * White balance colour temperature in Kelvin.
     * ~4000 K = warm candlelight.  ~6500 K = neutral daylight.  ~10000 K = icy blue.
     * Light world: ~5500 K (golden hour).  Shadow world: ~9000 K (cold moonlight).
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color Grading",
        meta = (ClampMin = "1500.0", ClampMax = "15000.0"))
    float WhiteTemp = 6500.f;

    /**
     * White balance tint offset (green/magenta axis).
     * Positive = magenta push.  Negative = green push.
     * Light world: slight magenta (+5) for warmth.  Shadow world: slight green (−5) for dread.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color Grading",
        meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float WhiteTint = 0.f;

    /** Overall colour saturation. 0 = greyscale | 1 = source | >1 = vivid. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color Grading",
        meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float ColorSaturation = 1.f;

    /** Screen-edge vignette.  0 = none | 1 = heavy. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Post Process",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float VignetteIntensity = 0.3f;

    /** Bloom brightness multiplier. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Post Process",
        meta = (ClampMin = "0.0"))
    float BloomIntensity = 0.675f;
};

// ─────────────────────────────────────────────────────────────────────────────
// UkdColorTheme — single source of truth for the Liminal Dusk palette.
//
// SETUP (do this once):
//   1.  Right-click in Content Browser → Miscellaneous → Data Asset.
//   2.  Choose UkdColorTheme as the class.  Name it DA_LiminalDusk.
//   3.  All default Liminal Dusk values are pre-baked by the constructor.
//   4.  Assign the asset to UkdWorldColorDriver::ColorTheme on your player BP.
//
// The asset is the only place colours live.  Every system (post-process, HUD,
// materials) reads from here — changing one hex value propagates everywhere.
// ─────────────────────────────────────────────────────────────────────────────

UCLASS(BlueprintType)
class THEPERSPVIEW_API UkdColorTheme : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UkdColorTheme();

    // ── Light World ──────────────────────────────────────────────────────────

    /** UI panel backgrounds, HUD base. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light World")
    FLinearColor Parchment;

    /** Light Health bar fill, key-pickup glow. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light World")
    FLinearColor Sunstone;

    /** Light damage flash, danger indicators.  Never reuse in shadow context. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light World")
    FLinearColor Ember;

    /** Light world text, outlines, inactive icons. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light World")
    FLinearColor DuskOak;

    // ── Shadow World ─────────────────────────────────────────────────────────

    /** Deep shadow background, fullscreen crush overlay. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shadow World")
    FLinearColor Void;

    /** Crush mode UI elements, portal mesh tint, shadow enemy outline. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shadow World")
    FLinearColor DeepIndigo;

    /** Shadow Stamina bar fill, shadow pickup glow. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shadow World")
    FLinearColor Spectre;

    /** Shadow world readable text, labels, tooltips. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shadow World")
    FLinearColor GhostLilac;

    // ── Transition & Feedback ────────────────────────────────────────────────

    /** Crush toggle VFX ONLY.  Reserved — never reuse for other systems. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
    FLinearColor CrushTeal;

    /** State_Exhausted screen flash, stamina bar danger pulse. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
    FLinearColor ExhaustRed;

    /** Score popups, coin ring shine, achievement flash. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
    FLinearColor ScoreGold;

    /** Neutral UI borders, disabled states, inactive buttons. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
    FLinearColor Slate;

    // ── Post-Process Profiles ────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Post Process")
    FkdPostProcessProfile LightWorldProfile;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Post Process")
    FkdPostProcessProfile ShadowWorldProfile;

    /** Seconds the post-process blend takes on each crush mode toggle. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Post Process",
        meta = (ClampMin = "0.05", ClampMax = "2.0"))
    float BlendDuration = 0.35f;

    // ── Material Parameter Collection ────────────────────────────────────────

    /**
     * Assign your MPC asset here (create one in the editor and name it
     * MPC_WorldColor).  UkdWorldColorDriver will write these parameters:
     *
     *   WorldBlendAlpha   (Scalar)  — 0 = light world, 1 = shadow world
     *   CrushModeColor    (Vector)  — lerped shadow tint
     *   LightWorldColor   (Vector)  — lerped light tint
     *
     * Any material in your project can then read these parameters to react to
     * world state — portals, pickup glows, shadow wall shaders, etc.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Material")
    TObjectPtr<UMaterialParameterCollection> WorldColorMPC;

    // ── Runtime Colour Helpers (callable from UMG / Blueprint) ───────────────

    /**
     * Stamina bar fill colour.
     * Full stamina = Spectre.  Lerps toward ExhaustRed below 30%.
     */
    UFUNCTION(BlueprintPure, Category = "Color Theme")
    FLinearColor GetStaminaBarColor(float FillFraction) const;

    /**
     * Health bar fill colour.
     * Full health = Sunstone.  Lerps toward Ember below 30%.
     */
    UFUNCTION(BlueprintPure, Category = "Color Theme")
    FLinearColor GetHealthBarColor(float FillFraction) const;

    /**
     * World-appropriate text colour.
     * DuskOak in light world; GhostLilac in crush mode.
     */
    UFUNCTION(BlueprintPure, Category = "Color Theme")
    FLinearColor GetWorldTextColor(bool bInCrushMode) const;

    /**
     * HUD panel background colour.
     * Parchment in light world; semi-transparent Void in crush mode.
     */
    UFUNCTION(BlueprintPure, Category = "Color Theme")
    FLinearColor GetHUDPanelColor(bool bInCrushMode) const;
};
