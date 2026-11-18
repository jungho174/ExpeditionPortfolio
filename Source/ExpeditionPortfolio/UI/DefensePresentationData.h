#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/QTEPresentationData.h"
#include "Combat/BattleGrounding.h"
#include "DefensePresentationData.generated.h"

class UAnimMontage;
class UNiagaraSystem;

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UDefensePresentationData : public UDataAsset
{
	GENERATED_BODY()
public:
	UDefensePresentationData() { DodgeGrounding.SampleSpacing=5; }
	UPROPERTY(BlueprintReadOnly,Category="Legacy") FQTESound SuccessSound;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") TObjectPtr<UNiagaraSystem> SuccessEffect;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") float SuccessEffectScale=.35f;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") float SuccessSlowDuration=0;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") float SuccessSlowFactor=.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit Reaction|Audio") FQTESound HitSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dodge|Audio") FQTESound DodgeSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parry")
	TObjectPtr<UAnimMontage> ParryMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parry", meta = (ClampMin = "0.0", Units = "s"))
	float ParryStartTime = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parry", meta = (ClampMin = "0.01"))
	float ParryPlayRate = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parry", meta = (ClampMin = "0.01", Units = "s"))
	float ParryDuration = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge")
	TObjectPtr<UAnimMontage> DodgeMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "0.0", Units = "cm"))
	float DodgeDistance = 140.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Grounding", meta = (DisplayName = "Grounding"))
	FBattleGroundingSettings DodgeGrounding;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "0.01", Units = "s"))
	float DodgeRetreatDuration = 0.14f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "0.01", Units = "s"))
	float DodgeReturnDuration = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "0.0", Units = "s"))
	float DodgeHoldAfterHit = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "0.01", Units = "s"))
	float DodgePoseTime = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Reaction")
	TObjectPtr<UAnimMontage> HitReactionMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Reaction", meta = (ClampMin = "0.01"))
	float HitReactionPlayRate = 1.0f;

	bool Validate(FString& Reason) const;
};
