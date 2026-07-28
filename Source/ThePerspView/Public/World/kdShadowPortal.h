// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "kdShadowPortal.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UkdPortalVortexComponent;
class AkdMyPlayer;
UCLASS()
class THEPERSPVIEW_API AkdShadowPortal : public AActor
{
	GENERATED_BODY()

public:
	AkdShadowPortal();

	// ── Visual ────────────────────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, Category = "Portal")
	TObjectPtr<UStaticMeshComponent> PortalMesh;

	// ── Trigger ───────────────────────────────────────────────────────────────

	UPROPERTY(EditDefaultsOnly, Category = "Portal")
	TObjectPtr<USphereComponent> TriggerSphere;

	// ── Configuration ─────────────────────────────────────────────────────────

	/** The other portal this one teleports the player to. Set in the editor. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Portal")
	TObjectPtr<AkdShadowPortal> LinkedPortal;

	/**
	 * Local-space offset applied at the exit portal so the player doesn't
	 * spawn inside the destination trigger sphere. Default pushes up (+Z).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Portal")
	FVector ExitOffset = FVector(0.f, 0.f, 175.f);

	/** Seconds both portals are disabled after a teleport. */
	UPROPERTY(EditDefaultsOnly, Category = "Portal", meta = (ClampMin = "0.1"))
	float CooldownDuration = 1.5f;

	/**
	 * VISIBILITY policy.
	 *   true  → portal reveals only while the player is shaded (State.Shaded),
	 *           in BOTH 2D and 3D, and is dormant/hidden in light. Direct
	 *           generalization of the old "reveal in Crush Mode" behavior.
	 *   false → portal is always visible & collidable; only the TELEPORT stays
	 *           shadow-gated. Use when discoverability matters more than mystery.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Portal")
	bool bRevealOnlyInShadow = true;

	// ── Blueprint Hooks ───────────────────────────────────────────────────────

	/** Fires on the SOURCE portal the moment the player teleports.
	*  Use for Niagara burst, warp sound, camera shake. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Portal")
	void BP_OnTeleportUsed(AkdMyPlayer* TeleportedPlayer);

	/** Fires on BOTH portals when a cooldown begins. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Portal")
	void BP_OnCooldownStarted();

	/** Fires when this portal's cooldown ends and it's ready again. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Portal")
	void BP_OnCooldownEnded();

	// ── Cooldown Visual ───────────────────────────────────────────────────────

	/** Mesh opacity applied to the material while on cooldown (0=invisible, 1=full).
	 *  Requires your portal material to have an "Opacity" scalar parameter. */
	UPROPERTY(EditDefaultsOnly, Category = "Portal", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CooldownMeshOpacity = 0.25f;

	/** Name of the scalar parameter on your portal mesh material for opacity control. */
	UPROPERTY(EditDefaultsOnly, Category = "Portal")
	FName OpacityParamName = FName("Opacity");

	// ── State (readable by linked portal) ────────────────────────────────────

	bool bCanTeleport = true;

	void OnPlayerArrived();   // called by the player's teleport component on arrival

	UkdPortalVortexComponent* GetVortex() const { return Vortex; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Portal")
	TObjectPtr<UkdPortalVortexComponent> Vortex;

	// ── Shadow-gated Visibility ────────────────────────────────────────────────

	/**
	 * Registered on the player's ASC in BeginPlay (when bRevealOnlyInShadow).
	 * Shows/hides the portal whenever State.Shaded is added or removed — works in
	 * both 2D and 3D because State.Shaded is dimension-agnostic.
	 */
	UFUNCTION()
	void OnShadedTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	/** Shows mesh + enables trigger. Called when the player becomes shaded. */
	void SetPortalActive(bool bActive);

	// ── Teleport ──────────────────────────────────────────────────────────────

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void EnableTeleport();

	UFUNCTION()
	void SetPortalCooldownVisual(bool bOnCooldown);

	FTimerHandle CooldownTimerHandle;

};


// AkdShadowPortal — a one-way teleporter gated on SHADOW, in BOTH 2D and 3D.
//
// Previously required State.CrushMode + State.InShadow (2D only). Now gates on
// the mode-agnostic State.Shaded tag written by UkdCrushStateComponent, so it
// works whenever the player stands in shadow — collapsed shadow plane OR a full
// 3D shadow cast by world geometry.
//
// SETUP (editor):
//   1. Place two AkdShadowPortal actors in your level.
//   2. On each one, set LinkedPortal to point to the other.
//   3. Adjust ExitOffset so the player doesn't spawn inside the destination
//      trigger sphere.
//
// Both portals go on cooldown simultaneously after a teleport, preventing the
// player from immediately bouncing back.
