#include "UI/HUD/PlayerHUDWidget.h"

#include "Combat/Health/HealthComponent.h"
#include "Components/ProgressBar.h"

void UPlayerHUDWidget::SetObservedHealthComponent(
    UHealthComponent *NewHealthComponent)
{
    if (ObservedHealthComponent.Get() == NewHealthComponent)
    {
        RefreshHealthDisplay();
        return;
    }

    if (UHealthComponent *PreviousHealthComponent =
            ObservedHealthComponent.Get())
    {
        PreviousHealthComponent->OnHealthChanged.RemoveDynamic(
            this,
            &UPlayerHUDWidget::HandleHealthChanged);
    }

    ObservedHealthComponent = NewHealthComponent;

    if (NewHealthComponent)
    {
        NewHealthComponent->OnHealthChanged.AddDynamic(
            this,
            &UPlayerHUDWidget::HandleHealthChanged);
    }

    RefreshHealthDisplay();
}

void UPlayerHUDWidget::NativeDestruct()
{
    SetObservedHealthComponent(nullptr);

    Super::NativeDestruct();
}

void UPlayerHUDWidget::HandleHealthChanged(
    float OldHealth,
    float NewHealth,
    float Delta)
{
    static_cast<void>(OldHealth);
    static_cast<void>(NewHealth);
    static_cast<void>(Delta);

    RefreshHealthDisplay();
}

void UPlayerHUDWidget::RefreshHealthDisplay()
{
    if (!HealthProgressBar)
    {
        return;
    }

    const UHealthComponent *HealthComponent =
        ObservedHealthComponent.Get();

    if (!HealthComponent)
    {
        HealthProgressBar->SetPercent(0.0f);
        return;
    }

    HealthProgressBar->SetPercent(
        FMath::Clamp(
            HealthComponent->GetHealthNormalized(),
            0.0f,
            1.0f));
}
