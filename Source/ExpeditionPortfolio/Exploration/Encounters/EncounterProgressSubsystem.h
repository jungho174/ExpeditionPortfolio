#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EncounterProgressSubsystem.generated.h"

// Session progress survives world travel. Disk persistence belongs to a future save system.
UCLASS()
class EXPEDITIONPORTFOLIO_API UEncounterProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="Encounter|Persistence") bool IsCleared(FName MapId, FGuid EncounterId) const;
	void MarkCleared(FName MapId, FGuid EncounterId);
private:
	TMap<FName,TSet<FGuid>> ClearedEncounters;
};
