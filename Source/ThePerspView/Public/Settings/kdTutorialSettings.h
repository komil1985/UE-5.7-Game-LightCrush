// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "kdTutorialSettings.generated.h"

class UkdTutorialBank;
class UkdTutorialPromptWidget;

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "kd Tutorial"))
class THEPERSPVIEW_API UkdTutorialSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }

    /** The lesson bank the tutorial subsystem reads from. */
    UPROPERTY(config, EditAnywhere, Category = "Tutorial", meta = (DisplayName = "Tutorial Bank"))
    TSoftObjectPtr<UkdTutorialBank> TutorialBank;

    /** WBP_TutorialPrompt. Loaded synchronously on first reveal. */
    UPROPERTY(config, EditAnywhere, Category = "Tutorial", meta = (DisplayName = "Prompt Widget Class"))
    TSoftClassPtr<UkdTutorialPromptWidget> PromptWidgetClass;

    /** Above the vitals HUD (5), below pause/menus (25+). */
    UPROPERTY(config, EditAnywhere, Category = "Tutorial")
    int32 PromptZOrder = 8;
};
