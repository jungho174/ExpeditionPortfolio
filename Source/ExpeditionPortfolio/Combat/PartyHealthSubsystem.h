#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PartyHealthSubsystem.generated.h"
class UCombatantComponent;

// Session state survives world travel; this deliberately does not write a save game.
UCLASS()
class EXPEDITIONPORTFOLIO_API UPartyHealthSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	bool RegisterCharacter(FName CharacterId,UCombatantComponent* Owner);
	void UnregisterCharacter(FName CharacterId,UCombatantComponent* Owner);
	bool FindHealth(FName CharacterId, float& OutHealth) const;
	void StoreHealth(FName CharacterId, float Health);
private:
	TMap<FName,TWeakObjectPtr<UCombatantComponent>> ActiveCharacters;
	TMap<FName, float> HealthByCharacter;
};
