#pragma once
#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "UI/BattlePresentationData.h"
#include "BattleCameraRig.generated.h"

UCLASS(Blueprintable)
class EXPEDITIONPORTFOLIO_API ABattleCameraRig : public ACameraActor
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Camera")
	void ConfigureShot(const FBattleCameraShot& Shot, AActor* Player, AActor* Enemy);
	UFUNCTION(BlueprintNativeEvent, Category="Combat|Camera")
	void ConfigureGroupShot(const FBattleCameraShot& Shot, AActor* Player, const TArray<AActor*>& Targets);
};
