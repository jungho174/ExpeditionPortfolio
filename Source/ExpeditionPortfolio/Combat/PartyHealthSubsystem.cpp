#include "Combat/PartyHealthSubsystem.h"
#include "Combat/CombatantComponent.h"
#include "GameFramework/Actor.h"

bool UPartyHealthSubsystem::RegisterCharacter(FName CharacterId,UCombatantComponent* Owner)
{
	if (CharacterId.IsNone() || !IsValid(Owner)) { return false; }
	const auto* Existing=ActiveCharacters.Find(CharacterId);
	if (Existing && Existing->IsValid() && Existing->Get()!=Owner)
	{
		UE_LOG(LogTemp,Error,TEXT("Duplicate party character ID '%s': %s and %s. HP ownership rejected."),*CharacterId.ToString(),*GetNameSafe(Existing->Get()->GetOwner()),*GetNameSafe(Owner->GetOwner()));
		return false;
	}
	ActiveCharacters.Add(CharacterId,Owner);return true;
}
void UPartyHealthSubsystem::UnregisterCharacter(FName CharacterId,UCombatantComponent* Owner)
{
	if (const auto* Existing=ActiveCharacters.Find(CharacterId);Existing && Existing->Get()==Owner) { ActiveCharacters.Remove(CharacterId); }
}

bool UPartyHealthSubsystem::FindHealth(FName CharacterId, float& OutHealth) const
{
	const float* Value = HealthByCharacter.Find(CharacterId);
	if (CharacterId.IsNone() || !Value) { return false; }
	OutHealth = *Value;
	return true;
}

void UPartyHealthSubsystem::StoreHealth(FName CharacterId, float Health)
{
	if (!CharacterId.IsNone() && FMath::IsFinite(Health)) { HealthByCharacter.Add(CharacterId, FMath::Max(0.f, Health)); }
}
