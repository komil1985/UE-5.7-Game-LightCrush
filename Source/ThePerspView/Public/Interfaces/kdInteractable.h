// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "kdInteractable.generated.h"

UINTERFACE(MinimalAPI)
class UkdInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class THEPERSPVIEW_API IkdInteractable
{
	GENERATED_BODY()

public:
	virtual void Interact(class AkdMyPlayer* InInstigator) = 0;
};
