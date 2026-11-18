#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/QTEPresentationData.h"
#include "PlayerUtilityPresentationData.generated.h"

class UAnimMontage;
class UNiagaraSystem;
class USoundBase;

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UPlayerUtilityPresentationData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly,Category="Legacy") FQTESound ApproachSound;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") FQTESound ReturnSound;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") TObjectPtr<UAnimMontage> MeleeMontage;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") TObjectPtr<UAnimMontage> ApproachMontage;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") TObjectPtr<UAnimMontage> ReturnMontage;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") float HitTime = 0.3f;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") float ApproachDuration = 0.55f;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") float ReturnDuration = 0.55f;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") float TargetDistance = 120.0f;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") float ApproachLeapHeight = 0;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") float ReturnLeapHeight = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Potion") TObjectPtr<UNiagaraSystem> PotionEffect;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Potion|Audio") TObjectPtr<USoundBase> PotionSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Potion|Audio", meta=(ClampMin="0.0", ClampMax="2.0")) float PotionSoundVolume = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Potion|Audio", meta=(ClampMin="0",ClampMax="5",Units="s")) float PotionSoundDelay = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Potion|Audio", meta=(ClampMin="0",Units="s")) float PotionSoundStartTime = 0;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") TObjectPtr<USoundBase> MeleeHitSound;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") float MeleeHitSoundVolume = 1;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") float MeleeHitSoundDelay = 0;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") float MeleeHitSoundStartTime = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Potion", meta=(ClampMin="0.01")) float PotionDuration = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Potion", meta=(ClampMin="0.01")) float PotionApplyTime = 0.5f;
	bool Validate(bool bPotion) const;
};
