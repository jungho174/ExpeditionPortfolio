#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BattleResultPresentationData.generated.h"

class UBattleResultWidget;

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UBattleResultPresentationData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Result") TSubclassOf<UBattleResultWidget> WidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Result", meta=(ClampMin="0",ClampMax="10",Units="s")) float SlowDuration=3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Result", meta=(ClampMin="0.01",ClampMax="1")) float SlowFactor=.15f;
};
