// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "kdMenuPresentationSubsystem.generated.h"

class UkdMenuCrushDirector;

/**
 * UkdMenuPresentationSubsystem
 *
 * Thin, per-world facade sitting between the main-menu UMG widget and the active
 * UkdMenuCrushDirector. The widget never holds a hard pointer to the diorama
 * actor; it asks the subsystem, which always resolves to whichever director is
 * live in the current menu world. This mirrors the project's established
 * subsystem-as-runtime-owner idiom (UkdThemeSubsystem, UkdAudioSubsystem) and
 * keeps the menu UI fully decoupled from level placement.
 *
 * All calls are safe no-ops when no director is registered, so widget code never
 * needs null-guarding around the diorama.
 */
UCLASS()
class THEPERSPVIEW_API UkdMenuPresentationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Called by the director on BeginPlay. The most recent registration wins. */
	void RegisterDirector(UkdMenuCrushDirector* InDirector);

	/** Called by the director on EndPlay. Only clears if this is the active one. */
	void UnregisterDirector(UkdMenuCrushDirector* InDirector);

	/** Light = false, Crush = true. Safe no-op if no director is present. */
	UFUNCTION(BlueprintCallable, Category = "Heliograph|Menu")
	void RequestCrush(bool bCrushed);

	UFUNCTION(BlueprintCallable, Category = "Heliograph|Menu")
	void ToggleCrush();

	UFUNCTION(BlueprintPure, Category = "Heliograph|Menu")
	bool IsCrushed() const;

	UFUNCTION(BlueprintPure, Category = "Heliograph|Menu")
	bool HasDirector() const { return ActiveDirector.IsValid(); }

private:
	/** Weak so a torn-down menu level never leaves us holding a dangling director. */
	TWeakObjectPtr<UkdMenuCrushDirector> ActiveDirector;
};
