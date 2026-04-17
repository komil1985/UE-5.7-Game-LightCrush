// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "kdMainMenuGameMode.generated.h"

/**
 * 
 */
UCLASS()
class THEPERSPVIEW_API AkdMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	AkdMainMenuGameMode();

	virtual void BeginPlay() override;
};


// ─────────────────────────────────────────────────────────────────────────────
// AkdMainMenuGameMode
//
// Stripped-down game mode used only by the Main Menu level.
// Shows the cursor, disables movement, and tells the HUD to display menus.
// ─────────────────────────────────────────────────────────────────────────────