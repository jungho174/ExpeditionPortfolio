#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/CharacterActionData.h"
#include "CharacterCombatData.generated.h"
class UCharacterItemUseData;
UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UCharacterCombatData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Character") TObjectPtr<UCharacterItemUseData> ItemUsePresentation;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Character") FName CharacterId;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Character") TObjectPtr<UCharacterActionData> BasicAttack;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Character") TArray<TObjectPtr<UCharacterActionData>> Skills;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Character") TObjectPtr<UCharacterActionData> ParryCounter;
	bool ContainsCommand(const UCharacterActionData* Action) const { return Action && (BasicAttack==Action || Skills.Contains(Action)); }
	bool IsValidConfiguration() const
	{
		if (CharacterId.IsNone() || !BasicAttack || !BasicAttack->IsValidAction() || !ParryCounter || !ParryCounter->IsValidAction()) { return false; }
		TSet<FName> Ids; Ids.Add(BasicAttack->ActionId);
		for (const UCharacterActionData* Skill:Skills) { if (!Skill || !Skill->IsValidAction() || Ids.Contains(Skill->ActionId)) { return false; } Ids.Add(Skill->ActionId); }
		return true;
	}
};
