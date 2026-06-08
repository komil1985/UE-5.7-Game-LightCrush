// Copyright ASKD Games


#include "UI/ColorLibrary/kdThemeAccess.h"
#include "Components/kdWorldColorDriver.h"
#include "Data/kdColorTheme.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

UkdWorldColorDriver* UkdThemeAccess::GetWorldColorDriver(const UObject* WidgetContext)
{
    if (!WidgetContext) return nullptr;

    UWorld* World = WidgetContext->GetWorld();
    if (!World) return nullptr;

    APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
    if (!Pawn) return nullptr;

    return Pawn->FindComponentByClass<UkdWorldColorDriver>();
}

UkdColorTheme* UkdThemeAccess::GetColorTheme(const UObject* WidgetContext)
{
    if (UkdWorldColorDriver* Driver = GetWorldColorDriver(WidgetContext))
    {
        return Driver->ColorTheme;
    }
    return nullptr;
}

float UkdThemeAccess::GetBlendAlpha(const UObject* WidgetContext)
{
    if (UkdWorldColorDriver* Driver = GetWorldColorDriver(WidgetContext))
    {
        return Driver->GetBlendAlpha();
    }
    return 0.f;
}