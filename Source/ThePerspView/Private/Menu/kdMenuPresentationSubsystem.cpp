// Copyright ASKD Games. ThePerspView.

#include "Menu/kdMenuPresentationSubsystem.h"
#include "Menu/kdMenuCrushDirector.h"


void UkdMenuPresentationSubsystem::RegisterDirector(UkdMenuCrushDirector* InDirector)
{
	if (IsValid(InDirector))
	{
		ActiveDirector = InDirector;
	}
}

void UkdMenuPresentationSubsystem::UnregisterDirector(UkdMenuCrushDirector* InDirector)
{
	// Only clear if the leaving director is the one we currently point at, so a
	// stale EndPlay can never wipe a freshly-registered director.
	if (ActiveDirector.Get() == InDirector)
	{
		ActiveDirector.Reset();
	}
}

void UkdMenuPresentationSubsystem::RequestCrush(bool bCrushed)
{
	if (UkdMenuCrushDirector* Director = ActiveDirector.Get())
	{
		Director->SetCrushed(bCrushed);
	}
}

void UkdMenuPresentationSubsystem::ToggleCrush()
{
	if (UkdMenuCrushDirector* Director = ActiveDirector.Get())
	{
		Director->ToggleCrush();
	}
}

bool UkdMenuPresentationSubsystem::IsCrushed() const
{
	if (const UkdMenuCrushDirector* Director = ActiveDirector.Get())
	{
		return Director->IsCrushed();
	}

	return false;
}
