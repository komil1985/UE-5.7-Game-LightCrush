// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdFallDamageComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEPERSPVIEW_API UkdFallDamageComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UkdFallDamageComponent();

    // ── Thresholds (cm/s downward velocity at landing) ────────────────────────

    /** Downward speed that triggers instant death. Default = ~6 m drop. */
    UPROPERTY(EditDefaultsOnly, Category = "Fall Damage", meta = (ClampMin = "100.0"))
    float FatalFallSpeed = 1200.f;

    /** Downward speed that triggers a hard-land shake without death.
     *  Set equal to FatalFallSpeed to disable soft warnings. */
    UPROPERTY(EditDefaultsOnly, Category = "Fall Damage", meta = (ClampMin = "100.0"))
    float HardLandSpeed = 800.f;

    // ── Override ──────────────────────────────────────────────────────────────

    /** If true, fatal falls are ignored while in Crush Mode (player is floating
     *  on the shadow plane, so gravity isn't dangerous there). */
    UPROPERTY(EditDefaultsOnly, Category = "Fall Damage")
    bool bIgnoreFallsInCrushMode = true;

    /** Blueprint hook for hard-land feedback (camera shake, grunt SFX, etc.). */
    UFUNCTION(BlueprintImplementableEvent, Category = "Fall Damage")
    void BP_OnHardLand(float ImpactSpeed);

    UPROPERTY(EditDefaultsOnly, Category = "Fall Damage")
    TObjectPtr<USoundBase> HardLandSound;

    UPROPERTY(EditDefaultsOnly, Category = "Fall Damage")
    TObjectPtr<USoundBase> FatalLandSound;

protected:
	virtual void BeginPlay() override;

private:
    UFUNCTION()
    void OnPlayerLanded(const FHitResult& Hit);

    UPROPERTY()
    TObjectPtr<class AkdMyPlayer> CachedPlayer;


		
};

// ─────────────────────────────────────────────────────────────────────────────
// UkdFallDamageComponent — attached to AkdMyPlayer.
//
// HOW IT WORKS:
//   • Binds to ACharacter::LandedDelegate in BeginPlay.
//   • On every landing, reads the Z component of the velocity at impact.
//   • If |VelocityZ| >= FatalFallSpeed → GM->HandlePlayerDeath()
//   • If |VelocityZ| >= HardLandSpeed  → plays a "hard land" feedback
//     (camera shake + sound) without killing — just a visual warning.
//
// TUNING REFERENCE (UE defaults, 1 UU = 1 cm):
//   Terminal velocity in UE free-fall is ~4000 cm/s.
//   A 3-meter drop hits ~770 cm/s.
//   A 10-meter drop hits ~1400 cm/s.
//   Recommended FatalFallSpeed: 1200 cm/s  (~6 m drop)
//
// SETUP:
//   Add to AkdMyPlayer constructor:
//     FallDamageComponent = CreateDefaultSubobject<UkdFallDamageComponent>(
//         TEXT("FallDamageComponent"));
//
//   Add to AkdMyPlayer.h:
//     UPROPERTY(EditDefaultsOnly, Category = "Components")
//     TObjectPtr<UkdFallDamageComponent> FallDamageComponent;
// ─────────────────────────────────────────────────────────────────────────────