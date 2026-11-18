#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/QTEPresentationData.h"
#include "CombatItemData.generated.h"
class UNiagaraSystem;

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UCombatItemData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Item") FName ItemId;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Item") FText DisplayName;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Item") FText Description;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Effect",meta=(ClampMin="0")) float HealingAmount=30;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Effect") TObjectPtr<UNiagaraSystem> Effect;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Effect") FQTESound UseSound;
	bool IsValidConfiguration() const { return !ItemId.IsNone() && FMath::IsFinite(HealingAmount) && HealingAmount>=0; }
};
