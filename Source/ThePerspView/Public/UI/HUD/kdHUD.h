// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "kdHUD.generated.h"

class UUserWidget;
/**
 * 
 */
UCLASS()
class THEPERSPVIEW_API AkdHUD : public AHUD
{
	GENERATED_BODY()
	
public:

protected:
	virtual void BeginPlay() override;
};
