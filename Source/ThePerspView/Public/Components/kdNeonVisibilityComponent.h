// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "kdNeonVisibilityComponent.generated.h"

/**
 * UkdNeonVisibilityComponent
 *
 * Attach to any Actor whose emissive / neon materials must remain
 * fully coloured while the rest of the world is desaturated in Crush Mode.
 *
 * HOW IT WORKS:
 *   Writes a Custom Depth Stencil value to every UPrimitiveComponent on
 *   the owning Actor.  The Crush-Mode Post Process Material reads that
 *   stencil and skips desaturation for any pixel that carries the tag.
 *
 * SETUP (one-time project config):
 *   Project Settings → Rendering → Postprocessing
 *     → "Custom Depth-Stencil Pass" = Enabled with Stencil
 *
 * SETUP (per-actor):
 *   1. Add this component to the Blueprint that has neon outlines.
 *   2. Leave StencilValue at 1 (or assign a unique value per category).
 *   3. In your Post Process Material use the node setup described in
 *      the .cpp file header comment.
 *
 * STENCIL CONVENTION (project-wide):
 *   0  — default (no special treatment, full B&W desaturation)
 *   1  — preserve colour          ← this component
 *   2+ — reserved for future use  (e.g. outline-only objects, UI elements)
 */


UCLASS(ClassGroup = "Rendering",meta = (BlueprintSpawnableComponent),DisplayName = "Neon Visibility (Crush Mode)")
class THEPERSPVIEW_API UkdNeonVisibilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UkdNeonVisibilityComponent();

    // ── Config ────────────────────────────────────────────────────────────────

        /**
         * Value written to CustomDepthStencilValue on every primitive.
         * Must match the value your Post Process Material tests against.
         * Default 1 = "preserve colour".
         */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Neon Visibility",
        meta = (ClampMin = "1", ClampMax = "255"))
    int32 StencilValue = 1;

    /**
     * When true the component tags all primitives automatically in BeginPlay.
     * Set false if you need deferred / conditional activation (call MarkMeshes
     * manually).
     */
    UPROPERTY(EditDefaultsOnly, Category = "Neon Visibility")
    bool bAutoApplyOnBeginPlay = true;

    // ── Public API ────────────────────────────────────────────────────────────

    /**
     * Writes the stencil value to every UPrimitiveComponent on the owning
     * Actor.  Safe to call multiple times; re-marks and updates the cache.
     */
    UFUNCTION(BlueprintCallable, Category = "Neon Visibility")
    void MarkMeshes();

    /**
     * Clears CustomDepth rendering on every previously marked primitive and
     * empties the cache.  Called automatically in EndPlay.
     */
    UFUNCTION(BlueprintCallable, Category = "Neon Visibility")
    void ClearMeshes();

    /**
     * Returns true if meshes are currently marked with the stencil value.
     */
    UFUNCTION(BlueprintPure, Category = "Neon Visibility")
    bool IsMeshMarked() const { return MarkedPrimitives.Num() > 0; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /**
     * Cached references so ClearMeshes can reset exactly what was tagged,
     * even if the Actor's component list changes at runtime.
     */
    UPROPERTY()
    TArray<TObjectPtr<UPrimitiveComponent>> MarkedPrimitives;

};
