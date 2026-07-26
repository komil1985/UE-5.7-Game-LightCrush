// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdPortalVortexComponent.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UAbilitySystemComponent;

/**
 * EkdPortalVortexPhase — the SHAPE state machine of the ink collapse.
 *
 * Shape (Charge) and availability (Opacity) are deliberately DECOUPLED:
 *   • Phase drives Charge  → 0 = open portal, 1 = collapsed singularity.
 *   • bOnCooldown drives Opacity independently → dim ≠ collapsed.
 * This lets a portal read as "spent but recovering" (dim + open) after a
 * teleport, instead of just fading out.
 */
UENUM()
enum class EkdPortalVortexPhase : uint8
{
	Dormant,     // Hidden by the actor; component does not tick.
	Idle,        // Armed & open. Swirl + breathing run in the material (no CPU tick).
	Collapsing,  // Teleport fired HERE (source): rapid inhale, Charge 0→1.
	Blooming     // Opening: Charge 1→0. Reached on show, on exit-arrival, and after a collapse.
};

/**
 * UkdPortalVortexComponent
 * ---------------------------------------------------------------------------
 * The SOLE writer of an AkdShadowPortal's dynamic material ("the ink drain").
 *
 * SINGLE-WRITER CONTRACT
 *   Once InitializeForMesh() runs, NOTHING else may call
 *   SetScalar/VectorParameterValueOnMaterials on the portal mesh. The actor's
 *   old SetPortalCooldownVisual() must delegate to SetCooldownState() here.
 *
 * TICK-GATING
 *   Perpetual swirl and idle "breathing" are authored in the MATERIAL (Time
 *   node) and cost zero CPU. This component only ticks while a transition is in
 *   flight (Collapsing/Blooming) or while the stamina-reactive glow is still
 *   lerping toward its target. When everything settles, the tick disables
 *   itself — mirroring UkdGameFeedbackComponent::NeedsTick().
 *
 * MATERIAL PARAMETER CONTRACT (only three — keep the surface tight)
 *   Scalar  "Charge"   [0..1] : 0 open portal → 1 collapsed pinpoint.
 *   Scalar  "Opacity"  [0..1] : availability dim (1 armed, CooldownOpacity spent).
 *   Vector  "GlowColor"       : event-horizon ring / filament tint (stamina-reactive).
 *
 * ORCHESTRATION (called by AkdShadowPortal)
 *   InitializeForMesh   once, in the portal's BeginPlay.
 *   OnPortalShown       CrushMode became active  → bloom open from a pinpoint.
 *   OnPortalHidden      CrushMode ended          → go Dormant, stop ticking.
 *   OnCollapse          player was swallowed HERE → inhale (source side).
 *   OnExitBloom         player arrived HERE       → bloom out (destination side).
 *   SetCooldownState    availability dim on/off  (replaces the old opacity write).
 *   OnCooldownComplete  cooldown timer elapsed   → brighten back to armed.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEPERSPVIEW_API UkdPortalVortexComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UkdPortalVortexComponent();

	/** Build the dynamic material on InMesh and cache the ASC for stamina-reactive
	 *  glow. Call ONCE from the owning portal's BeginPlay, after the ASC resolves.
	 *  InASC may be null (glow simply falls back to the theme's "full" colour). */
	void InitializeForMesh(UStaticMeshComponent* InMesh, UAbilitySystemComponent* InASC);

	// ── Orchestration hooks (driven by AkdShadowPortal) ──────────────────────
	void OnPortalShown();
	void OnPortalHidden();
	void OnCollapse();
	void OnExitBloom();
	void OnCooldownComplete();

	/** Availability dim. true = on cooldown (spent), false = armed. Independent
	 *  of the collapse shape. Replaces AkdShadowPortal::SetPortalCooldownVisual. */
	void SetCooldownState(bool bOnCooldown);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// ── Timing tunables ──────────────────────────────────────────────────────

	/** Seconds for the inhale (Charge 0→1) when a teleport fires here. Snappy. */
	UPROPERTY(EditAnywhere, Category = "Portal Vortex|Timing", meta = (ClampMin = "0.05"))
	float CollapseSeconds = 0.16f;

	/** Seconds for the bloom (Charge 1→0) on open / arrival / post-collapse reopen. */
	UPROPERTY(EditAnywhere, Category = "Portal Vortex|Timing", meta = (ClampMin = "0.05"))
	float BloomSeconds = 0.45f;

	/** How fast the ring glow chases the stamina-reactive target colour. */
	UPROPERTY(EditAnywhere, Category = "Portal Vortex|Timing", meta = (ClampMin = "0.1"))
	float GlowInterpSpeed = 6.f;

	/** How fast opacity eases between armed (1) and CooldownOpacity. */
	UPROPERTY(EditAnywhere, Category = "Portal Vortex|Timing", meta = (ClampMin = "0.1"))
	float OpacityInterpSpeed = 8.f;

	// ── Look tunables ────────────────────────────────────────────────────────

	/** Mesh opacity while spent. Mirrors the actor's old CooldownMeshOpacity. */
	UPROPERTY(EditAnywhere, Category = "Portal Vortex|Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CooldownOpacity = 0.25f;

	/** Custom-depth stencil stamped on the portal mesh so UkdWorldColorDriver
	 *  leaves its glow un-desaturated in the crush world. 1 = neon-preserve
	 *  (your convention). Set to 0 to opt out. */
	UPROPERTY(EditAnywhere, Category = "Portal Vortex|Look", meta = (ClampMin = "0", ClampMax = "255"))
	int32 NeonStencilValue = 1;

private:
	// ── State machine ────────────────────────────────────────────────────────
	void SetPhase(EkdPortalVortexPhase NewPhase);
	void RefreshTickState();
	bool NeedsTick() const;

	// ── Data flow ────────────────────────────────────────────────────────────
	float ReadStaminaFraction() const;      // 1.0 if no ASC / no max.
	FLinearColor ResolveTargetGlow() const; // Theme + stamina → ring colour.
	void PushParams() const;                // The ONE place that writes the MID.

	/** Fired by the ASC when ShadowStamina changes — kicks the tick so the glow
	 *  re-lerps even from an otherwise-idle (tick-off) state. */
	void OnStaminaChanged(const struct FOnAttributeChangeData& Data);

	// ── Cached refs ──────────────────────────────────────────────────────────
	UPROPERTY() TObjectPtr<UStaticMeshComponent> PortalMesh = nullptr;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DynMat = nullptr;
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	// ── Runtime ──────────────────────────────────────────────────────────────
	EkdPortalVortexPhase Phase = EkdPortalVortexPhase::Dormant;

	float Charge = 1.f;   // Start collapsed so the first show blooms open.
	float ChargeTarget = 1.f;
	float ChargeRate = 0.f;   // Units/sec for the active transition (constant lerp).

	float CurrentOpacity = 0.f;
	float TargetOpacity = 1.f;

	FLinearColor CurrentGlow = FLinearColor::White;

	bool bOnCooldown = false;

	// ── Material param names — edit if you rename them in M_ShadowPortal ──────
	static const FName MP_Charge;
	static const FName MP_Opacity;
	static const FName MP_GlowColor;
};
