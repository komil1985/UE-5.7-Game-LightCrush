// Copyright ASKD Games

#include "Data/kdColorTheme.h"

// ─────────────────────────────────────────────────────────────────────────────
// HexToLinear
//
// Converts an sRGB hex triple (uint8 components) to a linear FLinearColor.
// All Heliograph swatches below are defined in sRGB; gamma-correcting once
// here keeps every value perceptually accurate at runtime.
// ─────────────────────────────────────────────────────────────────────────────

static FLinearColor HexToLinear(uint8 R, uint8 G, uint8 B)
{
    return FLinearColor(
        FMath::Pow(R / 255.f, 2.2f),
        FMath::Pow(G / 255.f, 2.2f),
        FMath::Pow(B / 255.f, 2.2f),
        1.f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor — Heliograph defaults
//
// Every value here is overridable from the DA_Heliograph DataAsset.  The C++
// defaults are the canonical ones — tweak there for permanent changes, tweak
// in the asset for project-local tuning.
// ─────────────────────────────────────────────────────────────────────────────

UkdColorTheme::UkdColorTheme()
{
    // ── Light World ──────────────────────────────────────────────────────────
    Atmosphere = HexToLinear(0xE8, 0xDD, 0xC8);   // #E8DDC8  soft warm cream
    Stoneveil = HexToLinear(0xA8, 0x9F, 0x8A);   // #A89F8A  warm grey-tan
    Sunmark = HexToLinear(0xE8, 0xB8, 0x4B);   // #E8B84B  amber gold
    Inkbrown = HexToLinear(0x5A, 0x46, 0x32);   // #5A4632  dark warm brown

    // ── Crush World (Heliograph) ─────────────────────────────────────────────
    IndigoField = HexToLinear(0x0E, 0x15, 0x38);  // #0E1538  deep cobalt void
    Lumen = HexToLinear(0xF5, 0xE7, 0xB8);  // #F5E7B8  sun-bleached white-gold
    SolarWhite = HexToLinear(0xFC, 0xFA, 0xF0);  // #FCFAF0  exposed near-white
    PaleIon = HexToLinear(0xA8, 0xC0, 0xFF);  // #A8C0FF  pale cyan-violet

    // ── Feedback ─────────────────────────────────────────────────────────────
    EmberTrace = HexToLinear(0xFF, 0x6B, 0x47);   // #FF6B47  warm coral hazard
    ExhaustRed = HexToLinear(0xE8, 0x40, 0x40);   // #E84040  danger red
    GoldLeaf = HexToLinear(0xF0, 0xC0, 0x60);   // #F0C060  score shimmer
    Steelgrey = HexToLinear(0x4A, 0x4F, 0x5E);   // #4A4F5E  neutral cool grey

    // ── Light World Post-Process ─────────────────────────────────────────────
    // Late golden-hour daylight.  Subtle warmth without losing realism.
    LightWorldProfile.SceneColorTint = FLinearColor(1.02f, 1.00f, 0.95f);
    LightWorldProfile.WhiteTemp = 5800.f;
    LightWorldProfile.WhiteTint = 0.02f;
    LightWorldProfile.ColorSaturation = 1.00f;
    LightWorldProfile.VignetteIntensity = 0.20f;
    LightWorldProfile.BloomIntensity = 0.50f;
    LightWorldProfile.DepthOfFieldFstop = 5.6f;     // deep focus
    LightWorldProfile.DepthOfFieldSensorWidth = 35.f;     // standard
    LightWorldProfile.ChromaticAberrationIntensity = 0.f;  // clean, naturalistic

    // ── Crush World Post-Process (Heliograph) ────────────────────────────────
    // Deep indigo field, cool moonlight white-balance, heavy desaturation so
    // the lumen outlines and solar-white silhouettes carry all the colour.
    CrushWorldProfile.SceneColorTint = FLinearColor(0.55f, 0.65f, 1.10f);
    CrushWorldProfile.WhiteTemp = 8500.f;
    CrushWorldProfile.WhiteTint = -0.03f;
    CrushWorldProfile.ColorSaturation = 0.55f;
    CrushWorldProfile.VignetteIntensity = 0.45f;
    CrushWorldProfile.BloomIntensity = 1.10f;
    CrushWorldProfile.DepthOfFieldFstop = 2.8f;     // mild diorama softness
    CrushWorldProfile.DepthOfFieldSensorWidth = 80.f;     // slight telephoto compression
    CrushWorldProfile.ChromaticAberrationIntensity = 0.4f; // whisper of warm/cool fringe

    BlendDuration = 0.35f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Runtime helpers
// ─────────────────────────────────────────────────────────────────────────────

FLinearColor UkdColorTheme::GetStaminaBarColor(float FillFraction) const
{
    constexpr float DangerThreshold = 0.30f;

    if (FillFraction >= DangerThreshold)
    {
        return PaleIon;
    }

    // Empty (T=0) → ExhaustRed; reaches threshold (T=1) → full PaleIon
    const float T = FillFraction / DangerThreshold;
    return FLinearColor::LerpUsingHSV(ExhaustRed, PaleIon, T);
}

FLinearColor UkdColorTheme::GetHealthBarColor(float FillFraction) const
{
    constexpr float DangerThreshold = 0.30f;

    if (FillFraction >= DangerThreshold)
    {
        return Sunmark;
    }

    const float T = FillFraction / DangerThreshold;
    return FLinearColor::LerpUsingHSV(EmberTrace, Sunmark, T);
}

FLinearColor UkdColorTheme::GetWorldTextColor(bool bInCrushMode) const
{
    return bInCrushMode ? Lumen : Inkbrown;
}

FLinearColor UkdColorTheme::GetHUDPanelColor(bool bInCrushMode) const
{
    if (!bInCrushMode)
    {
        return Atmosphere;
    }

    // Semi-transparent IndigoField so the world's traced silhouettes
    // remain partially visible behind HUD panels in crush mode.
    FLinearColor CrushPanel = IndigoField;
    CrushPanel.A = 0.85f;
    return CrushPanel;
}

FLinearColor UkdColorTheme::GetOutlineGlowColor(bool bIsHazard) const
{
    return bIsHazard ? EmberTrace : Lumen;
}
