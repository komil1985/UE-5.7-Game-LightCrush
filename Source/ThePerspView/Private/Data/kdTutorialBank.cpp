// Copyright ASKD Games

#include "Data/kdTutorialBank.h"


const FkdTutorialStep* UkdTutorialBank::FindStep(FName StepId) const
{
    if (const int32* Idx = StepIndex.Find(StepId))
    {
        return Steps.IsValidIndex(*Idx) ? &Steps[*Idx] : nullptr;
    }
    return nullptr;
}

void UkdTutorialBank::PostLoad()
{
    Super::PostLoad();
    RebuildIndex();
}

#if WITH_EDITOR
void UkdTutorialBank::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    RebuildIndex();
}
#endif

void UkdTutorialBank::RebuildIndex()
{
    StepIndex.Reset();
    StepIndex.Reserve(Steps.Num());

    for (int32 i = 0; i < Steps.Num(); ++i)
    {
        const FName Id = Steps[i].StepId;
        if (Id.IsNone())
        {
            UE_LOG(LogTemp, Warning, TEXT("[kdTutorialBank] Step %d has no StepId — unreachable."), i);
            continue;
        }
        if (StepIndex.Contains(Id))
        {
            UE_LOG(LogTemp, Error, TEXT("[kdTutorialBank] Duplicate StepId '%s' at index %d — first wins."),
                *Id.ToString(), i);
            continue;
        }
        StepIndex.Add(Id, i);
    }
}