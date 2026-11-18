#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/BattlePresentationData.h"
#include "ActionCameraData.generated.h"
class UCharacterActionData;

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UActionCameraData : public UDataAsset
{
	GENERATED_BODY()
public:
	static const FBattleCameraShot& ResolveShot(const UCharacterActionData* Action,bool bPreparation,const FBattleCameraShot& CommonDefault);
	static const FBattleCameraShot& ResolveTargetShot(const UCharacterActionData* Action,const FBattleCameraShot& CommonDefault);
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Camera") bool bOverrideTargetSelection=false;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Camera",meta=(EditCondition="bOverrideTargetSelection")) FBattleCameraShot TargetSelectionShot;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Camera") bool bOverridePreparation=true;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Camera",meta=(EditCondition="bOverridePreparation")) FBattleCameraShot PreparationShot;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Camera") bool bOverrideExecution=true;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Camera",meta=(EditCondition="bOverrideExecution")) FBattleCameraShot ExecutionShot;
};
