#include "Player/MainPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"

void AMainPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalPlayerController())
	{
		return;
	}

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
			TEXT("MainPlayerController could not find the Enhanced Input subsystem."));

		return;
	}

	for (const FInputMappingContextConfig &MappingConfig : DefaultMappingContexts)
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
