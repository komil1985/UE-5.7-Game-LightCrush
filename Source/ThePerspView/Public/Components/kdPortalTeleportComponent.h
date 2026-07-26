// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdPortalTeleportComponent.generated.h"

class AkdMyPlayer;
class AkdShadowPortal;
class APlayerController;
class APlayerCameraManager;
class UCameraShakeBase;

/**
 * UkdPortalTeleportComponent  (lives on AkdMyPlayer)
 * ---------------------------------------------------------------------------
 * Turns the portal's instant SetActorLocation snap into a fade-covered
 * "swallow → move → exhale" teleport, so the camera never hard-cuts.
 *
 * WHY PLAYER-SIDE
 *   The sequence disables player input, drives the pawn camera fade, moves the
 *   capsule and re-applies velocity — all player-owned concerns. The portal
 *   only requests the move (BeginTeleport) and handles its own cooldown/VFX.
 *   This mirrors UkdDeathComponent, which owns the death fade on the player.
 *
 * SEQUENCE (all timers, no tick)
 *   BeginTeleport ─► fade OUT to FadeColor (held)   [FadeOutSeconds]
 *                 ─► HandleCovered: screen fully opaque
 *                        • move pawn to exit + re-apply in-plane velocity
 *                        • DestPortal->OnPlayerArrived()  (the exhale bloom)
 *                        • optional arrival camera shake
 *                 ─► (hold the black beat)            [HoldSeconds]
 *                 ─► HandleReveal: fade IN            [FadeInSeconds]
 *                 ─► FinishTeleport: restore input
 *
 * The whole thing is ~0.5s and reuses APlayerCameraManager::StartCameraFade,
 * the same call your death fade already relies on.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEPERSPVIEW_API UkdPortalTeleportComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UkdPortalTeleportComponent();

	/** Kick off a fade-covered teleport. Ignored if one is already running. */
	void BeginTeleport(const FVector& ExitLocation, AkdShadowPortal* DestinationPortal);

	bool IsTeleporting() const { return bTeleporting; }

	/** Hard-cancel (level transition / death mid-teleport). Restores input and
	 *  clears any held fade so we never leave the screen stuck on indigo. */
	void CancelTeleport();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	// ── Timing ───────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category = "Portal Teleport|Timing", meta = (ClampMin = "0.0"))
	float FadeOutSeconds = 0.16f;   // swallow

	UPROPERTY(EditAnywhere, Category = "Portal Teleport|Timing", meta = (ClampMin = "0.0"))
	float HoldSeconds = 0.04f;      // the covered beat

	UPROPERTY(EditAnywhere, Category = "Portal Teleport|Timing", meta = (ClampMin = "0.0"))
	float FadeInSeconds = 0.30f;    // exhale reveal

	// ── Look ─────────────────────────────────────────────────────────────────
	/** Fade colour. Default is a deep IndigoField so you're swallowed into the
	 *  crush-world's own darkness rather than a generic black. */
	UPROPERTY(EditAnywhere, Category = "Portal Teleport|Look")
	FLinearColor FadeColor = FLinearColor(0.006f, 0.009f, 0.050f, 1.f);

	/** Optional punch on arrival — wire a light UCameraShakeBase for impact. */
	UPROPERTY(EditAnywhere, Category = "Portal Teleport|Look")
	TSubclassOf<UCameraShakeBase> ArrivalShakeClass;

	// ── Behaviour ────────────────────────────────────────────────────────────
	/** Zero the collapse-axis (world X) velocity on exit, keeping in-plane
	 *  momentum so the player leaves the portal moving. Matches the old code.
	 *  NOTE: if you support non-X crush directions, zero along the crush basis
	 *  collapse normal from UkdCrushDirectionLibrary::MakeCrushBasis instead. */
	UPROPERTY(EditAnywhere, Category = "Portal Teleport|Behaviour")
	bool bZeroCollapseAxisVelocity = true;

	/** Freeze spring-arm lag during the covered move so camera lag can't swing
	 *  the view across the level as the fade lifts. Restored on finish. */
	UPROPERTY(EditAnywhere, Category = "Portal Teleport|Behaviour")
	bool bFreezeCameraLagDuringMove = true;

private:
	void HandleCovered();
	void HandleReveal();
	void FinishTeleport();

	void SetInputEnabled(bool bEnabled);
	void SetSpringArmLagEnabled(bool bEnabled);

	APlayerController* GetPC() const;
	APlayerCameraManager* GetCameraManager() const;

	UPROPERTY() TObjectPtr<AkdMyPlayer> CachedPlayer = nullptr;

	TWeakObjectPtr<AkdShadowPortal> PendingDest;
	FVector PendingExitLocation = FVector::ZeroVector;

	bool bTeleporting = false;

	// Camera-lag state captured at HandleCovered, restored at FinishTeleport.
	bool bSavedLocationLag = false;
	bool bSavedRotationLag = false;

	FTimerHandle CoveredTimer;
	FTimerHandle RevealTimer;
	FTimerHandle FinishTimer;
};
