#include "World/Encounters/EncounterDefinition.h"

#include "Misc/DataValidation.h"

#if WITH_EDITOR

EDataValidationResult UEncounterDefinition::IsDataValid(
    FDataValidationContext &Context) const
{
    EDataValidationResult Result =
        EDataValidationResult::Valid;

    if (EncounterId.IsNone())
    {
        Context.AddError(
            FText::FromString(
                TEXT(
                    "Encounter definition requires a non-empty "
                    "EncounterId.")));

        Result = EDataValidationResult::Invalid;
    }

    if (EncounterMap.IsNull())
    {
        Context.AddError(
            FText::FromString(
                TEXT(
                    "Encounter definition requires an "
                    "EncounterMap.")));

        Result = EDataValidationResult::Invalid;
    }

    return CombineDataValidationResults(
        Super::IsDataValid(Context),
        Result);
}

#endif
