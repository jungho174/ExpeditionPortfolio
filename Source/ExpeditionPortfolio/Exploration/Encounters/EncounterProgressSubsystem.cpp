#include "Exploration/Encounters/EncounterProgressSubsystem.h"
#include "Engine/World.h"

bool UEncounterProgressSubsystem::IsCleared(FName MapId, FGuid EncounterId) const
{
	if (MapId.IsNone() || !EncounterId.IsValid()) { return false; }
	const auto* Entries=ClearedEncounters.Find(FName(UWorld::RemovePIEPrefix(MapId.ToString())));
	return Entries && Entries->Contains(EncounterId);
}

void UEncounterProgressSubsystem::MarkCleared(FName MapId, FGuid EncounterId)
{
	if (!MapId.IsNone() && EncounterId.IsValid())
	{
		ClearedEncounters.FindOrAdd(FName(UWorld::RemovePIEPrefix(MapId.ToString()))).Add(EncounterId);
	}
}
