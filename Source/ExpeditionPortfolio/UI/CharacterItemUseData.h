#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CharacterItemUseData.generated.h"
class UAnimMontage;

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UCharacterItemUseData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Item Use") TObjectPtr<UAnimMontage> Montage;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Item Use",meta=(ClampMin="0.01")) float PlayRate=1;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Item Use",meta=(ClampMin="0.01",Units="s")) float ApplyTime=.5f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Item Use",meta=(ClampMin="0.02",Units="s")) float Duration=1.2f;
	bool IsValidConfiguration() const { return FMath::IsFinite(PlayRate) && PlayRate>0 && FMath::IsFinite(ApplyTime) && ApplyTime>0 && FMath::IsFinite(Duration) && Duration>ApplyTime; }
};
