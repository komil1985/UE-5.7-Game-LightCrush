// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "kdColorTheme.generated.h"

class UMaterialParameterCollection;

// ─────────────────────────────────────────────────────────────────────────────
// FkdPostProcessProfile
//
// Holds every post-process setting that differs between the Light World and
// the Crush (Shadow) World.  UkdWorldColorDriver lerps between two of these
// every frame while a blend is in progress.
//
// DOF is part of "what the world looks like in this state" so it lives here,
// not in the transition component — single source of truth.
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FkdPostProcessProfile
{
    GENERATED_BODY()

    // ── Tint ──────────────────────────────────────────────────────────────────

    /** Multiplicative scene tint.  Near-neutral in light; pushed indigo in crush. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color Grading")
    FLinearColor SceneColorTint = FLinearColor(1.f, 1.f, 1.f, 1.f);

    /**
     * White balance colour temperature in Kelvin.
     * 5800 K = late golden-hour daylight (light world).
     * 8500 K = cool, papery moonlight (crush world).
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color Grading",
        meta = (ClampMin = "1500.0", ClampMax = "15000.0"))
    float WhiteTemp = 6500.f;

    /**
     * White balance tint offset (green/magenta axis).
     * Positive = magenta, negative = green.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color Grading",
        meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float WhiteTint = 0.f;

    /**
     * Global saturation multiplier.
     * 1.0 = source colours.  Light world stays ~1.0; crush drops to ~0.55
     * so the indigo field reads as flat and graphic rather than colourful.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color Grading",
        meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float ColorSaturation = 1.f;

    /** Screen-edge vignette.  Crush mode runs heavier to focus on traced silhouettes. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Post Process",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float VignetteIntensity = 0.3f;

    /** Bloom brightness multiplier — boosted in crush so the lumen outlines glow. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Post Process",
        meta = (ClampMin = "0.0"))
    float BloomIntensity = 0.55f;

    // ── Depth of Field (centralised — used to live on the transition component) ─

    /**
     * Camera f-stop.  Lower = more blur.
     * Light world stays deep-focused (5.6+); crush mode opens to ~2.8 for a
     * mild diorama softness without losing edge legibility.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Depth of Field",
        meta = (ClampMin = "0.5", ClampMax = "22.0"))
    float DepthOfFieldFstop = 5.6f;

    /** Simulated sensor width (mm) used together with f-stop for blur calculation. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Depth of Field",
        meta = (ClampMin = "1.0"))
    float DepthOfFieldSensorWidth = 35.f;

    // ── Chromatic Aberration (steady-state baseline) ────────────────────────────
    //
    // Drives FPostProcessSettings::SceneFringeIntensity.  This is the *baseline*
    // value used while no event burst is active.  Transient bursts fired from
    // kdGameFeedbackComponent ride additively on top of this through the
    // WorldColorDriver's TriggerChromaticBurst() API, so there's still only one
    // writer on the SceneFringeIntensity field.
    //
    // Heliograph defaults:
    //   Light world : 0.0  — clean, naturalistic.
    //   Crush world : 0.4  — a whisper of warm/cool channel split, selling the
    //                        "exposed sun-print" aesthetic without looking broken.
    //
    // Range guidance (UE5): 0.5 = subtle, 2-3 = strong, 5+ = dizzying.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Post Process",
        meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float ChromaticAberrationIntensity = 0.f;
};

// ─────────────────────────────────────────────────────────────────────────────
// UkdColorTheme — single source of truth for the "Heliograph" palette.
//
// The Heliograph aesthetic:
//   The light world is naturalistic, sun-touched, slightly warm.
//   The crush world is a luminous blueprint — a 19th-century sun-print of
//   the 3D world.  Silhouettes trace in a warm white-gold "Lumen" line against
//   a deep cobalt "IndigoField".  The player and interactable props read as
//   pure white "SolarWhite" — the only fully exposed silhouettes in the frame.
//
// SETUP (do this once in the editor):
//   1.  Right-click in Content Browser → Miscellaneous → Data Asset.
//   2.  Choose UkdColorTheme as the class.  Name it DA_Heliograph.
//   3.  All Heliograph default values are pre-baked by the constructor.
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
    // Naturalistic, sun-touched.  These are the colours of the 3D world.

    /** UI panel backgrounds, HUD base — soft warm cream. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light World")
    FLinearColor Atmosphere;

    /** Neutral surface mid-tone — stone, wood, terrain. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light World")
    FLinearColor Stoneveil;

    /** Health bar fill, key-pickup glow, sun-warmed accent — amber gold. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light World")
    FLinearColor Sunmark;

    /** Light-world text, outlines, inactive icons — dark warm brown. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Light World")
    FLinearColor Inkbrown;

    // ── Crush World (Heliograph) ─────────────────────────────────────────────
    // The luminous blueprint — light & outline as the only visual language.

    /** Deep cobalt void — the background field of the shadow plane. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crush World")
    FLinearColor IndigoField;

    /**
     * Sun-bleached white-gold — the "exposed" outline trace.
     * Drive this on edge-glow material params for walls, props, geometry.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crush World")
    FLinearColor Lumen;

    /**
     * Pure exposed silhouette — player, interactables, important pickups.
     * Reads as the "fully sun-printed" element in the frame.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crush World")
    FLinearColor SolarWhite;

    /** Stamina bar fill, shadow energy, portal glow — pale cyan-violet. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crush World")
    FLinearColor PaleIon;

    // ── Cross-World Feedback ─────────────────────────────────────────────────
    // Universal accents that read in both light and crush modes.

    /** Hazard outline / damage flash — warm coral, alarms the eye in either world. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
    FLinearColor EmberTrace;

    /** Critical-state pulse — danger red. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
    FLinearColor ExhaustRed;

    /** Score popup, coin shine, achievement flash — warm shimmer. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
    FLinearColor GoldLeaf;

    /** Neutral UI borders, disabled states, inactive buttons. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
    FLinearColor Steelgrey;

    // ── Post-Process Profiles ────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Post Process")
    FkdPostProcessProfile LightWorldProfile;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Post Process")
    FkdPostProcessProfile CrushWorldProfile;

    /** Seconds the post-process blend takes on each crush-mode toggle. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Post Process",
        meta = (ClampMin = "0.05", ClampMax = "2.0"))
    float BlendDuration = 0.35f;

    // ── Material Parameter Collection ────────────────────────────────────────

    /**
     * Create an MPC named MPC_WorldColor with these parameters:
     *
     *   Scalars:
     *     WorldBlendAlpha       — 0 = light world, 1 = crush
     *     EdgePulseAlpha        — transient 0..1 burst on shadow entry
     *
     *   Vectors:
     *     LumenColor            — outline trace colour (lerped)
     *     SolarColor            — exposed silhouette colour (lerped)
     *     IndigoFieldColor      — crush background colour (lerped)
     *
     * Any material in the project can read these to react to world state —
     * edge-trace materials, portal glows, exposed-silhouette stencils, etc.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Material")
    TObjectPtr<UMaterialParameterCollection> WorldColorMPC;

    // ── Runtime Colour Helpers ───────────────────────────────────────────────

    /** Full stamina = PaleIon.  Lerps toward ExhaustRed below 30 %. */
    UFUNCTION(BlueprintPure, Category = "Color Theme")
    FLinearColor GetStaminaBarColor(float FillFraction) const;

    /** Full health = Sunmark.  Lerps toward EmberTrace below 30 %. */
    UFUNCTION(BlueprintPure, Category = "Color Theme")
    FLinearColor GetHealthBarColor(float FillFraction) const;

    /** Inkbrown in light world; Lumen in crush world (matches outline trace). */
    UFUNCTION(BlueprintPure, Category = "Color Theme")
    FLinearColor GetWorldTextColor(bool bInCrushMode) const;

    /**
     * HUD panel background.
     * Light world → Atmosphere (warm cream, opaque).
     * Crush world → IndigoField (semi-transparent so traced edges show through).
     */
    UFUNCTION(BlueprintPure, Category = "Color Theme")
    FLinearColor GetHUDPanelColor(bool bInCrushMode) const;

    /** Outline glow colour — Lumen for normal geometry, EmberTrace for hazards. */
    UFUNCTION(BlueprintPure, Category = "Color Theme")
    FLinearColor GetOutlineGlowColor(bool bIsHazard) const;
};
