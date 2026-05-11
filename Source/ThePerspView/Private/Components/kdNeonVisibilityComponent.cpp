// Copyright ASKD Games


#include "Components/kdNeonVisibilityComponent.h"


UkdNeonVisibilityComponent::UkdNeonVisibilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


void UkdNeonVisibilityComponent::MarkMeshes()
{
    // Clear any previous state to avoid double-tagging on repeated calls.
    ClearMeshes();

    AActor* Owner = GetOwner();
    if (!ensureMsgf(Owner, TEXT("NeonVisibility: No owner actor — component not attached correctly.")))
    {
        return;
    }

    TArray<UPrimitiveComponent*> Primitives;
    Owner->GetComponents<UPrimitiveComponent>(Primitives);

    if (Primitives.IsEmpty())
    {
#if !UE_BUILD_SHIPPING
        UE_LOG(LogTemp, Warning,
            TEXT("NeonVisibility [%s]: No UPrimitiveComponents found on owner. "
                "Make sure the component is on an Actor that has a mesh."),
            *Owner->GetName());
#endif
        return;
    }

    for (UPrimitiveComponent* Prim : Primitives)
    {
        if (!IsValid(Prim))
        {
            continue;
        }

        // Enable custom depth rendering on this primitive.
        Prim->SetRenderCustomDepth(true);

        // Write our stencil tag value. The post process material reads this
        // to decide whether to apply desaturation or preserve colour.
        Prim->SetCustomDepthStencilValue(StencilValue);

        // ERSM_Default writes to all 8 stencil bits — compatible with UE's
        // built-in outline system if you ever add one later.
        Prim->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);

        MarkedPrimitives.Add(Prim);
    }

#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Log,
        TEXT("NeonVisibility [%s]: Marked %d primitive(s) with StencilValue=%d"),
        *Owner->GetName(),
        MarkedPrimitives.Num(),
        StencilValue);
#endif
}

void UkdNeonVisibilityComponent::ClearMeshes()
{
    for (TObjectPtr<UPrimitiveComponent>& Prim : MarkedPrimitives)
    {
        if (IsValid(Prim.Get()))
        {
            Prim->SetRenderCustomDepth(false);
            Prim->SetCustomDepthStencilValue(0);
        }
    }

    MarkedPrimitives.Reset();
}

void UkdNeonVisibilityComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoApplyOnBeginPlay)
	{
		MarkMeshes();
	}	
}

void UkdNeonVisibilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearMeshes();
	Super::EndPlay(EndPlayReason);
}
