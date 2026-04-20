// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "kdHazard.generated.h"


// ─────────────────────────────────────────────────────────────────────────────
// EHazardBehavior — controls when the hazard is lethal.
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EHazardBehavior : uint8
{
	// Always kills on contact regardless of player state
	Always          UMETA(DisplayName = "Always Lethal"),

	// Only kills while player is in Crush Mode (shadow plane hazards)
	CrushModeOnly   UMETA(DisplayName = "Crush Mode Only"),

	// Only kills in normal 3D mode (surface hazards)
	NormalModeOnly  UMETA(DisplayName = "Normal Mode Only"),
};


class UBoxComponent;
class UStaticMeshComponent;
UCLASS(BlueprintType, Blueprintable)
class THEPERSPVIEW_API AkdHazard : public AActor
{
	GENERATED_BODY()
	
public:	
	AkdHazard();

    // ── Config ────────────────────────────────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hazard")
    EHazardBehavior Behavior = EHazardBehavior::Always;

    /** Seconds after killing a player before this hazard can kill again.
     *  Prevents multiple death calls if the player overlaps the edge. */
    UPROPERTY(EditDefaultsOnly, Category = "Hazard", meta = (ClampMin = "0.1"))
    float ImmunityDuration = 2.f;

    UPROPERTY(EditDefaultsOnly, Category = "Hazard")
    TObjectPtr<USoundBase> HazardSound;

    // ── Blueprint hooks ───────────────────────────────────────────────────────

    /** Override for death VFX (sparks, gore, sound swell, etc.). */
    UFUNCTION(BlueprintImplementableEvent, Category = "Hazard")
    void BP_OnPlayerKilled();

protected:
	virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> HazardMesh;

    /** Resize this box to cover the lethal area. */
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UBoxComponent> HazardVolume;

private:	
    bool bOnCooldown = false;
    FTimerHandle ImmunityTimerHandle;

    UFUNCTION()
    void OnHazardVolumeOverlap(
        UPrimitiveComponent* OverlappedComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32                 OtherBodyIndex,
        bool                  bFromSweep,
        const FHitResult& SweepResult);

    bool IsLethalToPlayer(class AkdMyPlayer* Player) const;
    void ResetCooldown();

};


