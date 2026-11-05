#include "Encounters/Pedro/PedroOpportunityState.h"

bool FPedroOpportunityState::Configure(
    const TArray<FPedroOpportunityDefinition>& Definitions)
{
    TSet<FName> SeenIds;
    TArray<FPedroOpportunityProgressEntry> NewEntries;
    NewEntries.Reserve(Definitions.Num());

    for (const FPedroOpportunityDefinition& Definition : Definitions)
    {
        if (Definition.OpportunityId.IsNone() ||
            SeenIds.Contains(Definition.OpportunityId))
        {
            return false;
        }

        SeenIds.Add(Definition.OpportunityId);

        FPedroOpportunityProgressEntry Entry;
        Entry.OpportunityId = Definition.OpportunityId;
        Entry.Tier = Definition.Tier;
        NewEntries.Add(Entry);
    }

    Entries = MoveTemp(NewEntries);
    return true;
}

FPedroOpportunityProgressEntry* FPedroOpportunityState::FindMutable(
    FName OpportunityId)
{
    if (OpportunityId.IsNone())
    {
        return nullptr;
    }

    for (FPedroOpportunityProgressEntry& Entry : Entries)
    {
        if (Entry.OpportunityId == OpportunityId)
        {
            return &Entry;
        }
    }
    return nullptr;
}

const FPedroOpportunityProgressEntry* FPedroOpportunityState::Find(
    FName OpportunityId) const
{
    if (OpportunityId.IsNone())
    {
        return nullptr;
    }

    for (const FPedroOpportunityProgressEntry& Entry : Entries)
    {
        if (Entry.OpportunityId == OpportunityId)
        {
            return &Entry;
        }
    }
    return nullptr;
}

bool FPedroOpportunityState::MarkPlayerExposed(FName OpportunityId)
{
    FPedroOpportunityProgressEntry* Entry = FindMutable(OpportunityId);
    if (!Entry || Entry->bPlayerExposed)
    {
        return false;
    }

    Entry->bPlayerExposed = true;
    return true;
}

bool FPedroOpportunityState::NoteAcquiredResponse(FName OpportunityId)
{
    FPedroOpportunityProgressEntry* Entry = FindMutable(OpportunityId);
    if (!Entry || Entry->bPedroAcquiredResponse)
    {
        return false;
    }

    Entry->bPedroAcquiredResponse = true;
    return true;
}

int32 FPedroOpportunityState::GetCoreCount() const
{
    int32 Count = 0;
    for (const FPedroOpportunityProgressEntry& Entry : Entries)
    {
        if (Entry.Tier == EPedroOpportunityTier::Core)
        {
            ++Count;
        }
    }
    return Count;
}

int32 FPedroOpportunityState::GetSampledCoreCount() const
{
    int32 Count = 0;
    for (const FPedroOpportunityProgressEntry& Entry : Entries)
    {
        if (Entry.Tier == EPedroOpportunityTier::Core && Entry.IsSampled())
        {
            ++Count;
        }
    }
    return Count;
}

int32 FPedroOpportunityState::GetBonusCount() const
{
    return Entries.Num() - GetCoreCount();
}

int32 FPedroOpportunityState::GetSampledBonusCount() const
{
    int32 Count = 0;
    for (const FPedroOpportunityProgressEntry& Entry : Entries)
    {
        if (Entry.Tier == EPedroOpportunityTier::Bonus && Entry.IsSampled())
        {
            ++Count;
        }
    }
    return Count;
}

int32 FPedroOpportunityState::GetRequiredCoreSamples(float Fraction) const
{
    const int32 TotalCore = GetCoreCount();
    if (TotalCore == 0)
    {
        return 0;
    }

    // Fraction is additionally editor-clamped on the Director. Keep this
    // pure-data API safe when invoked directly by tests or other native code.
    const float Clamped = FMath::Clamp(Fraction, 0.01f, 1.0f);
    return FMath::Clamp(FMath::CeilToInt(Clamped * TotalCore), 1, TotalCore);
}

bool FPedroOpportunityState::HasSufficientCoreSamples(float Fraction) const
{
    return GetCoreCount() > 0 &&
        GetSampledCoreCount() >= GetRequiredCoreSamples(Fraction);
}

void FPedroOpportunityState::Reset()
{
    Entries.Reset();
}
