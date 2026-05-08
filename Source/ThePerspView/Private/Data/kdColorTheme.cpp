// Copyright ASKD Games

#include "Data/kdColorTheme.h"

// ─────────────────────────────────────────────────────────────────────────────
// Internal helper
//
// Converts an sRGB hex triple (as uint8 components) to a linear FLinearColor.
// Unreal's colour pickers and the Liminal Dusk swatches are defined in sRGB,
// so gamma-correcting here keeps every value perceptually accurate at runtime.
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
// Constructor — Liminal Dusk defaults
//
// Every value here matches the swatches in the colour guide.  Override any of
// them in the DA_LiminalDusk DataAsset if you want to tweak without recompiling.
// ─────────────────────────────────────────────────────────────────────────────

UkdColorTheme::UkdColorTheme()
{
    // ── Light World ──────────────────────────────────────────────────────────
    Parchment = HexToLinear(0xFA, 0xF0, 0xDC);   // #FAF0DC  warm off-white
    Sunstone = HexToLinear(0xE8, 0xB8, 0x4B);   // #E8B84B  amber gold
    Ember = HexToLinear(0xC4, 0x5E, 0x1A);   // #C45E1A  burnt sienna
    DuskOak = HexToLinear(0x7A, 0x5C, 0x2E);   // #7A5C2E  warm brown

    // ── Shadow World ─────────────────────────────────────────────────────────
    Void = HexToLinear(0x12, 0x08, 0x30);   // #120830  near-black indigo
    DeepIndigo = HexToLinear(0x4A, 0x2D, 0x8F);   // #4A2D8F  deep violet
    Spectre = HexToLinear(0x8B, 0x5C, 0xF6);   // #8B5CF6  electric purple
    GhostLilac = HexToLinear(0xC4, 0xA8, 0xFF);   // #C4A8FF  pale lavender

    // ── Feedback ─────────────────────────────────────────────────────────────
    CrushTeal = HexToLinear(0x00, 0xC4, 0x9A);   // #00C49A  teal (transition only)
    ExhaustRed = HexToLinear(0xE8, 0x40, 0x40);   // #E84040  danger red
    ScoreGold = HexToLinear(0xF0, 0xC0, 0x60);   // #F0C060  score shimmer
    Slate = HexToLinear(0x3A, 0x3A, 0x4A);   // #3A3A4A  neutral grey-blue

    // ── Light World Post-Process ──────────────────────────────────────────────
    // Warm, golden-hour feel.  5500 K temperature, slight magenta push,
    // boosted saturation, shallow vignette, normal bloom.
    LightWorldProfile.SceneColorTint = FLinearColor(1.05f, 0.98f, 0.88f);
    LightWorldProfile.WhiteTemp = 5500.f;
    LightWorldProfile.WhiteTint = 0.05f;
    LightWorldProfile.ColorSaturation = 1.10f;
    LightWorldProfile.VignetteIntensity = 0.25f;
    LightWorldProfile.BloomIntensity = 0.50f;

    // ── Shadow World Post-Process ─────────────────────────────────────────────
    // Cold, oppressive, slightly green-tinged dread.  9000 K temperature,
    // heavy desaturation, deep vignette, boosted bloom for glow effects.
    ShadowWorldProfile.SceneColorTint = FLinearColor(0.82f, 0.78f, 1.05f);
    ShadowWorldProfile.WhiteTemp = 9000.f;
    ShadowWorldProfile.WhiteTint = -0.05f;
    ShadowWorldProfile.ColorSaturation = 0.75f;
    ShadowWorldProfile.VignetteIntensity = 0.55f;
    ShadowWorldProfile.BloomIntensity = 1.20f;

    BlendDuration = 0.35f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Runtime helpers
// ─────────────────────────────────────────────────────────────────────────────

FLinearColor UkdColorTheme::GetStaminaBarColor(float FillFraction) const
{
    // Below 30 % the bar transitions from Spectre toward ExhaustRed
    constexpr float DangerThreshold = 0.30f;

    if (FillFraction >= DangerThreshold)
    {
        return Spectre;
    }

    // T = 0 at empty → full ExhaustRed; T = 1 at threshold → full Spectre
    const float T = FillFraction / DangerThreshold;
    return FLinearColor::LerpUsingHSV(ExhaustRed, Spectre, T);
}

FLinearColor UkdColorTheme::GetHealthBarColor(float FillFraction) const
{
    constexpr float DangerThreshold = 0.30f;

    if (FillFraction >= DangerThreshold)
    {
        return Sunstone;
    }

    const float T = FillFraction / DangerThreshold;
    return FLinearColor::LerpUsingHSV(Ember, Sunstone, T);
}

FLinearColor UkdColorTheme::GetWorldTextColor(bool bInCrushMode) const
{
    return bInCrushMode ? GhostLilac : DuskOak;
}

FLinearColor UkdColorTheme::GetHUDPanelColor(bool bInCrushMode) const
{
    if (!bInCrushMode)
    {
        return Parchment;
    }

    // Semi-transparent Void — dark enough to be distinct but readable in shadow
    FLinearColor CrushPanel = Void;
    CrushPanel.A = 0.85f;
    return CrushPanel;
}
