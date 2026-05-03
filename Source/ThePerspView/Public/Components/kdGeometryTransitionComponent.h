// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdGeometryTransitionComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// UkdWorldGeometryTransitionComponent
//
// Drop on any floor, wall or platform actor. Subscribes to the player's
// State.CrushMode tag.  When the mode changes it runs a three-step sequence:
//
//   1. SHIVER  — rapid Y/Z oscillation, amplitude decays to zero.
//                Reads as the geometry "cracking" before it moves.
//
//   2. MORPH   — lerps the actor's X world position toward CrushWorldX and
//                squashes its X scale to CrushXScaleMultiplier.
//                Result: geometry slides flat onto the shadow plane and
//                becomes paper-thin — a visible 3D → 2D world shift.
//                On crush EXIT the sequence runs in reverse.
//
//   3. SNAP    — at morph end, values are hard-set and tick is disabled.
//
// OnTransitionComplete fires the state update BEFORE the settle, so the
// visual morph never blocks gameplay.
//
// SETUP
//   1. Add component to BP_Floor / BP_Wall / BP_Platform.
//   2. Set CrushWorldX to the X coordinate of your shadow plane (usually 0).
//   3. Tune ShiverAmplitude and CrushXScaleMultiplier in Details.
//   4. No curve assets required — fully self-contained tick math.
// ─────────────────────────────────────────────────────────────────────────────


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class THEPERSPVIEW_API UkdGeometryTransitionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UkdGeometryTransitionComponent();

    // ── Morph targets ─────────────────────────────────────────────────────────

/** World X coordinate the geometry slides to in Crush (2D) mode.
 *  Set to the X position of your shadow wall — usually 0. */
    UPROPERTY(EditInstanceOnly, Category = "Crush | Geometry")
    float CrushWorldX = 0.f;

    /** How thin the geometry's X axis becomes in Crush mode.
     *  0.04 = 4 % of original X extent (paper-thin slab). */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Geometry",
        meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float CrushXScaleMultiplier = 0.04f;

    /** Seconds for the main slide + squash. Slightly longer than the player
     *  transition gives the world a satisfying "catching up" lag. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Geometry",
        meta = (ClampMin = "0.1"))
    float MorphDuration = 0.6f;

    // ── Shiver ────────────────────────────────────────────────────────────────

    /** Duration of the shiver phase before the main slide (seconds).
     *  Set to 0 to skip. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Geometry | Shiver",
        meta = (ClampMin = "0.0", ClampMax = "0.3"))
    float ShiverDuration = 0.09f;

    /** Peak Y/Z displacement of the shiver (cm). */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Geometry | Shiver",
        meta = (ClampMin = "0.0", ClampMax = "40.0"))
    float ShiverAmplitude = 7.f;

    /** Oscillation cycles per second during shiver. */
    UPROPERTY(EditDefaultsOnly, Category = "Crush | Geometry | Shiver",
        meta = (ClampMin = "1.0", ClampMax = "60.0"))
    float ShiverFrequency = 24.f;

protected:
	virtual void BeginPlay() override;	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UFUNCTION()
    void OnCrushModeTagChanged(const FGameplayTag Tag, int32 NewCount);

    // ── Cached mesh (first UStaticMeshComponent on the owner) ─────────────────
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> CachedMesh;

    // ── Internal state machine ────────────────────────────────────────────────

    enum class EGeoState : uint8 { Idle, Shivering, Morphing };
    EGeoState State = EGeoState::Idle;
    bool      bToCrushMode = false;
    float     StateElapsed = 0.f;

    // ── Baselines captured at BeginPlay ───────────────────────────────────────

    FVector OriginalMeshRelativeLoc;
    FVector OriginalMeshRelativeScale;

    // ── Morph FROM / TO (in mesh-local relative space) ────────────────────────
    // X position: we convert CrushWorldX into a mesh-relative delta at BeginPlay.
    float MeshRelX_Original = 0.f;    // original mesh relative X
    float MeshRelX_Crush = 0.f;    // relative X that maps mesh to CrushWorldX

    // X scale: relative scale values to lerp between
    float MeshRelScaleX_Original = 1.f;
    float MeshRelScaleX_Crush = 0.04f;
};
