// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "kdThemeAccess.generated.h"

class UkdColorTheme;
class UkdWorldColorDriver;
class APlayerController;

/**
 * Helpers for UMG widgets to fetch the active ColorTheme and WorldColorDriver.
 *
 * Designed to be safe to call every frame from property bindings: returns
 * nullptr quietly if anything in the chain isn't ready yet (e.g. player not
 * yet possessed, driver's BeginPlay hasn't run). Callers should null-check.
 */
UCLASS()
class THEPERSPVIEW_API UkdThemeAccess : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Resolves the player's WorldColorDriver from any widget context.
     * @param WidgetContext  Any UObject in the widget — usually `this`.
     */
    UFUNCTION(BlueprintPure, Category = "Heliograph",
        meta = (WorldContext = "WidgetContext", DefaultToSelf = "WidgetContext"))
    static UkdWorldColorDriver* GetWorldColorDriver(const UObject* WidgetContext);

    /** Convenience: GetWorldColorDriver → ColorTheme. Null-safe. */
    UFUNCTION(BlueprintPure, Category = "Heliograph",
        meta = (WorldContext = "WidgetContext", DefaultToSelf = "WidgetContext"))
    static UkdColorTheme* GetColorTheme(const UObject* WidgetContext);

    /** Convenience: GetWorldColorDriver → GetBlendAlpha(). 0 if no driver. */
    UFUNCTION(BlueprintPure, Category = "Heliograph",
        meta = (WorldContext = "WidgetContext", DefaultToSelf = "WidgetContext"))
    static float GetBlendAlpha(const UObject* WidgetContext);
};
