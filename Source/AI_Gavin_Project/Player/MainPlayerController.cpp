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

	ULocalPlayer* LocalPlayer = GetLocalPlayer();

	if (!LocalPlayer)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("MainPlayerController could not find a LocalPlayer.")
		);

		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
			LocalPlayer
		);

	if (!InputSubsystem)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("MainPlayerController could not find the Enhanced Input subsystem.")
		);

		return;
	}

	if (!DefaultMappingContext)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("MainPlayerController has no default input mapping context.")
		);

		return;
	}

	InputSubsystem->AddMappingContext(
		DefaultMappingContext,
		0
	);
}
