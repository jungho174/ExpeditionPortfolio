#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/QTEPresentationData.h"
#include "ParryFeedbackData.generated.h"

class UNiagaraSystem;
class UCameraShakeBase;
USTRUCT(BlueprintType)
struct FParryFeedbackStyle
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FQTESound Sound;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TObjectPtr<UNiagaraSystem> Effect;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector EffectScale=FVector(.35f);
 UPROPERTY(BlueprintReadOnly,Category="Legacy") FName Socket=TEXT("hand_l");
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0",ClampMax="2",Units="s")) float SlowDuration=0;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0.01",ClampMax="1")) float SlowFactor=.15f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TSubclassOf<UCameraShakeBase> CameraShake;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0",ClampMax="2")) float CameraShakeScale=.25f;
};
// Shared success feedback, independent of the character's counter action and its weapon.
UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UParryFeedbackData : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Parry") FParryFeedbackStyle Intermediate;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Parry") FParryFeedbackStyle Complete;
};
