#include "Player/MainPlayerController.h"

#include "Combat/Characters/CombatantCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "UI/HUD/PlayerHUDWidget.h"

void AMainPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalPlayerController())
	{
		return;
	}

	InitializeInputMappingContexts();
	InitializePlayerHUD();
}

void AMainPlayerController::OnPossess(APawn *InPawn)
{
	Super::OnPossess(InPawn);

	RefreshPlayerHUDDataSource();
}

void AMainPlayerController::OnUnPossess()
{
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetObservedHealthComponent(nullptr);
	}

	Super::OnUnPossess();
}

void AMainPlayerController::InitializeInputMappingContexts()
{
	ULocalPlayer *LocalPlayer = GetLocalPlayer();

	if (!LocalPlayer)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("MainPlayerController could not find a LocalPlayer."));

		return;
	}

	UEnhancedInputLocalPlayerSubsystem *InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
			LocalPlayer);

	if (!InputSubsystem)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"MainPlayerController could not find the "
				"Enhanced Input subsystem."));

		return;
	}

	for (const FInputMappingContextConfig &MappingConfig :
		 DefaultMappingContexts)
	{
		if (!MappingConfig.MappingContext)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT(
					"MainPlayerController has an invalid "
					"default input mapping context."));

			continue;
		}

		InputSubsystem->AddMappingContext(
			MappingConfig.MappingContext,
			MappingConfig.Priority);
	}
}

void AMainPlayerController::InitializePlayerHUD()
{
	if (!PlayerHUDWidgetClass)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"MainPlayerController has no player HUD "
				"widget class configured."));

		return;
	}

	PlayerHUDWidget =
		CreateWidget<UPlayerHUDWidget>(
			this,
			PlayerHUDWidgetClass);

	if (!PlayerHUDWidget)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"MainPlayerController failed to create "
				"the player HUD widget."));

		return;
	}

	PlayerHUDWidget->AddToPlayerScreen();

	RefreshPlayerHUDDataSource();
}

void AMainPlayerController::RefreshPlayerHUDDataSource()
{
	if (!PlayerHUDWidget)
	{
		return;
	}

	ACombatantCharacter *Combatant =
		Cast<ACombatantCharacter>(GetPawn());

	PlayerHUDWidget->SetObservedHealthComponent(
		Combatant
			? Combatant->GetHealthComponent()
			: nullptr);
}
