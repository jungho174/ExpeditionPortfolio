#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/CombatItemData.h"
#include "BattleRulesData.generated.h"
USTRUCT(BlueprintType)
struct FCombatItemStock
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere,BlueprintReadOnly) TObjectPtr<UCombatItemData> Item;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0",ClampMax="99")) int32 Quantity=3;
};
UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UBattleRulesData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Turn",meta=(ClampMin="0",ClampMax="5",Units="s")) float TurnTransitionDelay=1;
	// Shared party stock for a new encounter. Retry restores this encounter's entry snapshot.
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Items") TArray<FCombatItemStock> InitialItems;
	bool IsValidConfiguration() const
	{
		if (!FMath::IsFinite(TurnTransitionDelay) || TurnTransitionDelay<0 || TurnTransitionDelay>5) { return false; }
		TSet<FName> Ids;
		for (const auto& Stock:InitialItems)
		{
			if (!Stock.Item || !Stock.Item->IsValidConfiguration() || Stock.Quantity<0 || Stock.Quantity>99 || Ids.Contains(Stock.Item->ItemId)) { return false; }
			Ids.Add(Stock.Item->ItemId);
		}
		return true;
	}
};
