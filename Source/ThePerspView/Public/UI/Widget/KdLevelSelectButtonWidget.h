// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "KdLevelSelectButtonWidget.generated.h"


class UButton;
class UTextBlock;
class UImage;
class UWidgetSwitcher;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKdLevelButtonClicked, int32, LevelIndex);
/**
 * 
 */
UCLASS()
class THEPERSPVIEW_API UKdLevelSelectButtonWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// Fills in this entry's label and lock/completed visuals, and stores the index for OnClicked.
	UFUNCTION(BlueprintCallable, Category = "Level Select")
	void Setup(int32 InLevelIndex, const FText& InDisplayName, bool bUnlocked, bool bCompleted);

	UPROPERTY(BlueprintAssignable, Category = "Level Select")
	FOnKdLevelButtonClicked OnClicked;

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleButtonClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SelectButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LevelLabelText;

	// Optional: swap between an "unlocked" (index 0) and "locked" (index 1) visual state.
	// Use this OR the individual LockIcon/CompletedCheckmark images below, whichever fits
	// your art better - you don't need both.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> LockStateSwitcher;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> LockIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> CompletedCheckmark;

	int32 LevelIndex = INDEX_NONE;
};
