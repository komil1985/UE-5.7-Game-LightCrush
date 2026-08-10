// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "kdActivatable.generated.h"


UINTERFACE(MinimalAPI)
class UkdActivatable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class THEPERSPVIEW_API IkdActivatable
{
	GENERATED_BODY()

public:
	virtual void Activate(AActor* Instigator) = 0;
};
