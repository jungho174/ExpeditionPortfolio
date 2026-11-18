#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/ActionQTEPatternData.h"
#include "CharacterActionData.generated.h"
class UActionCameraData;

UENUM(BlueprintType)
enum class ECharacterActionTargetScope : uint8
{
	SingleEnemy,
	AllEnemies
};

// Shared action rules. Concrete subclasses describe their own animation/weapon execution.
UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UCharacterActionData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Action|Camera") TObjectPtr<UActionCameraData> CameraData;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Action") FName ActionId;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Action") FText DisplayName;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Action",meta=(MultiLine="true")) FText Description;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Action|Combat",meta=(ClampMin="0")) float BaseDamage=25;
	// Shared targeting rule; character assets decide which attacks use it.
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Action|Combat") ECharacterActionTargetScope TargetScope=ECharacterActionTargetScope::SingleEnemy;
	// Zero means unlimited; each character has an independent use ledger.
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Action|Combat",meta=(ClampMin="0")) int32 UsesPerEncounter=0;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Action|Execution") FName ExecutorComponentName;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Action|QTE") bool bEnableTimingBonus=false;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Action|QTE",meta=(EditCondition="bEnableTimingBonus")) TObjectPtr<UActionQTEPatternData> QTEPattern;
	// Serialized name retained when migrating existing magic assets.
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Action|Preparation",meta=(ClampMin="0.1",DisplayName="Preparation Duration")) float ChargeDuration=2.5f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Action|QTE",meta=(ClampMin="0",ClampMax="5",Units="s",EditCondition="bEnableTimingBonus")) float PostTimingHoldDuration=0;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,AdvancedDisplay,Category="Action|QTE",meta=(ClampMin="0.01")) float TimingSuccessDuration=.3f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,AdvancedDisplay,Category="Action|QTE",meta=(ClampMin="1",ClampMax="10")) float TimingDamageMultiplier=1.5f;
	float GetQTEDuration() const { return QTEPattern && QTEPattern->IsValidConfiguration()?QTEPattern->GetDuration():ChargeDuration; }
	float GetPreparationDuration() const { return (bEnableTimingBonus?GetQTEDuration():ChargeDuration)+(bEnableTimingBonus && FMath::IsFinite(PostTimingHoldDuration)?FMath::Clamp(PostTimingHoldDuration,0.f,5.f):0); }
	bool IsValidAction() const { return !ActionId.IsNone() && FMath::IsFinite(BaseDamage) && BaseDamage>=0 && UsesPerEncounter>=0
		&& (TargetScope==ECharacterActionTargetScope::SingleEnemy || TargetScope==ECharacterActionTargetScope::AllEnemies); }
};
