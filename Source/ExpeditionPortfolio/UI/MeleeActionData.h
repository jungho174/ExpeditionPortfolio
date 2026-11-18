#pragma once
#include "CoreMinimal.h"
#include "Combat/CharacterActionData.h"
#include "Combat/BattleGrounding.h"
#include "UI/QTEPresentationData.h"
#include "MeleeActionData.generated.h"
class UAnimMontage;
UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UMeleeActionData : public UCharacterActionData
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Melee") TObjectPtr<UAnimMontage> AttackMontage;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Melee") TObjectPtr<UAnimMontage> ApproachMontage;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Melee") TObjectPtr<UAnimMontage> ReturnMontage;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Melee") TObjectPtr<UAnimMontage> PreparationMontage;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Melee",meta=(ClampMin="0.001",Units="s")) float HitTime=.3f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Melee",meta=(ClampMin="0.01",Units="s")) float ApproachDuration=.55f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Melee",meta=(ClampMin="0.01",Units="s")) float ReturnDuration=.55f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Melee",meta=(ClampMin="1",Units="cm")) float TargetDistance=120;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Melee",meta=(ClampMin="0",Units="cm")) float ApproachLeapHeight=0;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Melee",meta=(ClampMin="0",Units="cm")) float ReturnLeapHeight=0;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Melee|Grounding") FBattleGroundingSettings Grounding;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Melee|Audio") FQTESound ApproachSound;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Melee|Audio") FQTESound ReturnSound;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Melee|Audio") FQTESound HitSound;
	bool IsValidPresentation() const;
};
