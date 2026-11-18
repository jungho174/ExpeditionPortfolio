#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/BattleEnemyActor.h"
#include "EncounterGroupData.generated.h"

USTRUCT(BlueprintType)
struct FEncounterGroupMember
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere,BlueprintReadWrite) TSubclassOf<ABattleEnemyActor> EnemyClass;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) FTransform RelativeTransform;
};

// Reusable composition and formation. Health stays on each spawned monster.
UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UEncounterGroupData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FEncounterGroupMember> Members;
	UFUNCTION(BlueprintPure,Category="Encounter") bool IsValidComposition() const
	{
		if (Members.IsEmpty()) { return false; }
		for (const auto& Member:Members)
		{
			if (!Member.EnemyClass || Member.RelativeTransform.ContainsNaN() || Member.RelativeTransform.GetScale3D().GetMin()<=0) { return false; }
		}
		return true;
	}
};
