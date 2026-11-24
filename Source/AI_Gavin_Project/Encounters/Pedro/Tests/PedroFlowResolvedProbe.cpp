#include "Encounters/Pedro/Tests/PedroFlowResolvedProbe.h"
#include "World/Encounters/EncounterDefinition.h"

void UPedroFlowResolvedProbe::OnEncounterResolved(UEncounterDefinition* Encounter)
{
    ++NotificationCount;
    LastEncounterId = IsValid(Encounter) ? Encounter->GetEncounterId() : NAME_None;
}
