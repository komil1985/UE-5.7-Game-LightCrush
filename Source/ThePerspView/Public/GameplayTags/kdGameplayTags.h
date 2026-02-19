// Copyright ASKD Games

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * kdGameplayTags
 * 
 * Singleton containing native gameplay tags
 */

struct FkdGameplayTags
{
public:
	static const FkdGameplayTags& Get() { return GameplayTags; }

	static void InitializeNativeGameplayTags();

protected:

private:
	static FkdGameplayTags GameplayTags;
};