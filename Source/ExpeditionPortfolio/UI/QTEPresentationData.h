#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "QTEPresentationData.generated.h"
class USoundBase;

USTRUCT(BlueprintType)
struct FQTESound
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<USoundBase> Sound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0",ClampMax="2")) float Volume = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0",ClampMax="5",Units="s")) float Delay = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0",Units="s")) float StartTime = 0;
};

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UQTEPresentationData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="QTE|Audio") FQTESound StartSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="QTE|Audio") FQTESound RegularSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="QTE|Audio") FQTESound PerfectSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="QTE|Audio") FQTESound MissSound;
};
