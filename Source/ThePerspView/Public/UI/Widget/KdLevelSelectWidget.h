// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "KdLevelSelectWidget.generated.h"


class UPanelWidget;
class UKdLevelSelectButtonWidget;
class UButton;
/**
 * 
 */
UCLASS()
class THEPERSPVIEW_API UKdLevelSelectWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// Optional player-facing names, keyed by the map's FName as it appears in LevelOrder
	// (e.g. "L_Level_01" -> "The Descent"). Any level with no entry here is auto-labelled
	// "Level N" by its position among the playable levels.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Select")
	TMap<FName, FText> DisplayNameOverrides;

	UPROPERTY(EditDefaultsOnly, Category = "Level Select")
	TSubclassOf<UKdLevelSelectButtonWidget> LevelButtonClass;

	// Only used when LevelButtonContainer is a UniformGridPanel / GridPanel. Ignored for
	// VerticalBox / HorizontalBox / WrapBox, which flow their children automatically.
	UPROPERTY(EditAnywhere, Category = "Level Select")
	int32 ColumnsPerRow = 3;

	// Rebuilds the list from current save state. Runs automatically on construct; call it
	// again if you re-show a cached instance without recreating it.
	UFUNCTION(BlueprintCallable, Category = "Level Select")
	void RefreshLevelButtons();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	// Carries the LevelOrder index of the chosen level.
	UFUNCTION()
	void HandleLevelButtonClicked(int32 LevelOrderIndex);

	UFUNCTION()
	void HandleBackClicked();

	// UniformGridPanel / GridPanel / WrapBox / VerticalBox / HorizontalBox all work.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> LevelButtonContainer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackButton;
};
