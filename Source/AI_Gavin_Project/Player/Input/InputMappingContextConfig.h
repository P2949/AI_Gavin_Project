#pragma once

#include "CoreMinimal.h"
#include "InputMappingContextConfig.generated.h"

class UInputMappingContext;

USTRUCT(BlueprintType)
struct AI_GAVIN_PROJECT_API FInputMappingContextConfig
{
        GENERATED_BODY()

        UPROPERTY(
            EditAnywhere,
            BlueprintReadOnly,
            Category = "Input")
        TObjectPtr<UInputMappingContext> MappingContext;

        UPROPERTY(
            EditAnywhere,
            BlueprintReadOnly,
            Category = "Input")
        int32 Priority = 0;
};
