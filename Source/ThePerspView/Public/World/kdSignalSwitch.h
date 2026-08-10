// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/kdInteractable.h"
#include "kdSignalSwitch.generated.h"


UENUM(BlueprintType)
enum class EkdActivationMode : uint8
{
    Both       UMETA(DisplayName = "Both (2D + 3D)"),
    CrushOnly  UMETA(DisplayName = "Crush Mode only (2D)"),
    LightOnly  UMETA(DisplayName = "Light World only (3D)")
};

class USphereComponent;
class UStaticMeshComponent;
UCLASS()
class THEPERSPVIEW_API AkdSignalSwitch : public AActor, public IkdInteractable
{
	GENERATED_BODY()
	
public:	
	AkdSignalSwitch();

    // IkdInteractable — the player pokes the switch, not the blocker.
    virtual void Interact(class AkdMyPlayer* InInstigator) override;

    // ── Mesh ─────────────────────────────────────────────────────────────────
    UPROPERTY(EditDefaultsOnly, Category = "Mesh")
    TObjectPtr<UStaticMeshComponent> SwitchMesh;

    /** Overlap reach so the player's ECC_Pawn interact query returns this switch. */
    UPROPERTY(EditDefaultsOnly, Category = "Interaction")
    TObjectPtr<USphereComponent> InteractionSphere;

    // ── Wiring ─────────────────────────────────────────────────────────────────
    /**
     * The devices this switch drives. The editor only lets you assign actors
     * that implement IkdActivatable (MustImplement). Set per instance.
     */
    UPROPERTY(EditInstanceOnly, Category = "Signal", 
        meta = (MustImplement = "/Script/ThePerspView.kdActivatable"))
    TArray<TObjectPtr<AActor>> Targets;

    /** Which world(s) this switch responds in. Defaults to both. */
    UPROPERTY(EditAnywhere, Category = "Signal")
    EkdActivationMode ActivationMode = EkdActivationMode::Both;

private:
    /** True if the player's current world matches ActivationMode. */
    bool PassesModeGate(class AkdMyPlayer* Player) const;

};
