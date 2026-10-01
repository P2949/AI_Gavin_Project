#include "World/Encounters/EncounterProgressSubsystem.h"

#include "World/Encounters/EncounterDefinition.h"

bool UEncounterProgressSubsystem::ResolveEncounter(
    UEncounterDefinition *Encounter)
{
    if (!IsValid(Encounter))
    {
        return false;
    }

    const FName EncounterId =
        Encounter->GetEncounterId();

    if (EncounterId.IsNone() ||
        ResolvedEncounterIds.Contains(EncounterId))
    {
        return false;
    }

    ResolvedEncounterIds.Add(EncounterId);

    OnEncounterResolved.Broadcast(Encounter);

    return true;
}

bool UEncounterProgressSubsystem::IsEncounterResolved(
    UEncounterDefinition *Encounter) const
{
    if (!IsValid(Encounter))
    {
        return false;
    }

    const FName EncounterId =
        Encounter->GetEncounterId();

    return !EncounterId.IsNone() &&
           ResolvedEncounterIds.Contains(EncounterId);
}

void UEncounterProgressSubsystem::ResetRun()
{
    ResolvedEncounterIds.Reset();
}
