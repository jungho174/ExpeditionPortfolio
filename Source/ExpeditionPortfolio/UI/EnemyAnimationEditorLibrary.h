#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EnemyAnimationEditorLibrary.generated.h"
class UAnimBlueprint;
class UAnimSequence;
class UAnimMontage;
UCLASS()
class EXPEDITIONPORTFOLIO_API UEnemyAnimationEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable,Category="Combat|Editor") static bool BuildIdleGraph(UAnimBlueprint* Blueprint,UAnimSequence* Idle);
	UFUNCTION(BlueprintCallable,Category="Combat|Editor") static bool BuildComboMontage(UAnimMontage* Montage,const TArray<UAnimSequence*>& Clips,const TArray<float>& Rates,const TArray<float>& HitTimes,UAnimSequence* LeadPose);
};
