// Copyright ASKD Games


#include "UI/Widget/kdLoadingScreenWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

// Tips shown in rotation — edit freely
const TArray<FString> UkdLoadingScreenWidget::LoadingHints =
{
    TEXT("Step into the shadow to enter Crush Mode."),
    TEXT("Dash direction is based on your last steering input."),
    TEXT("Running out of stamina will knock you out of Crush Mode."),
    TEXT("Interact with glowing objects while in Crush Mode."),
    TEXT("Shadow portals only activate when you're standing in shadow."),
    TEXT("Enemies patrol the shadow plane — time your movements carefully."),
    TEXT("Your stamina regenerates faster when you stand still."),
    TEXT("Light blockers can be moved to reveal new shadow paths."),
};

void UkdLoadingScreenWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ElapsedTime = 0.f;
    FakeProgress = 0.f;
    HintIndex = FMath::RandRange(0, LoadingHints.Num() - 1);

    if (Bar_Progress)
    {
        Bar_Progress->SetPercent(0.f);
    }

    AdvanceHint();

    // Cycle hints every 3 seconds
    GetWorld()->GetTimerManager().SetTimer(
        HintCycleHandle, this, &UkdLoadingScreenWidget::AdvanceHint,
        3.0f, /*bLoop=*/true);
}

void UkdLoadingScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    ElapsedTime += InDeltaTime;
    UpdateProgressBar(InDeltaTime);
}

void UkdLoadingScreenWidget::StartAnimation()
{
    ElapsedTime = 0.f;
    FakeProgress = 0.f;
}

void UkdLoadingScreenWidget::AdvanceHint()
{
    if (LoadingHints.IsEmpty()) return;

    HintIndex = (HintIndex + 1) % LoadingHints.Num();

    if (Txt_LoadingHint)
    {
        Txt_LoadingHint->SetText(FText::FromString(LoadingHints[HintIndex]));
    }
}

void UkdLoadingScreenWidget::UpdateProgressBar(float DeltaTime)
{
    if (!Bar_Progress) return;

    // Fake-fill the bar: fast up to 80 %, then slow — level completes the rest
    const float TargetPct = ElapsedTime < MinDisplayTime ? 0.8f : 1.0f;
    const float FillRate = (FakeProgress < 0.8f) ? 0.4f : 0.05f;

    FakeProgress = FMath::Min(TargetPct, FakeProgress + FillRate * DeltaTime);
    Bar_Progress->SetPercent(FakeProgress);
}
