#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BattlePresentationData.generated.h"

class ABattleCameraRig;
class USoundBase;

USTRUCT(BlueprintType)
struct FBattleCameraShot
{
	GENERATED_BODY()
	// X: player toward enemy, Y: right of that direction, Z: world up. Origin: player capsule center.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Offset = FVector(-280, 90, 110);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOriginAtEnemy = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bFitTargetBounds = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite,meta=(ClampMin="1",ClampMax="2")) float TargetFramingPadding = 1.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0",ClampMax="1")) float LookAtEnemyWeight = 0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LookHeight = 45;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="20",ClampMax="100")) float FieldOfView = 60;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAvoidWorldObstacles = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1",ClampMax="30")) float CollisionRadius = 12;
};

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UBattlePresentationData : public UDataAsset
{
	GENERATED_BODY()
public:
	UBattlePresentationData()
	{
		TargetSelectionShot.bOriginAtEnemy = true;
		TargetSelectionShot.Offset = FVector(-450, 70, 75);
		TargetSelectionShot.LookAtEnemyWeight = 1;
		TargetSelectionShot.LookHeight = 45;
		TargetSelectionShot.FieldOfView = 48;
	}
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FBattleCameraShot TargetSelectionShot;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FBattleCameraShot AllyTargetSelectionShot;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Target Selection|Audio") TObjectPtr<USoundBase> TargetSelectionSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Target Selection|Audio", meta=(ClampMin="0",ClampMax="2")) float TargetSelectionSoundVolume = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Target Selection|Audio", meta=(ClampMin="0",ClampMax="5",Units="s")) float TargetSelectionSoundDelay = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Target Selection|Audio", meta=(ClampMin="0",Units="s")) float TargetSelectionSoundStartTime = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<ABattleCameraRig> CameraRigClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0",ClampMax="3")) float BlendDuration = 0.35f;
	// Additional time in the restored shoulder view before beginning the chosen attack.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Target Selection", meta=(ClampMin="0",ClampMax="2",Units="s")) float TargetConfirmHoldDuration = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.05",ClampMax="5")) float IntroDuration = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FBattleCameraShot CommandShot;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FBattleCameraShot PreparationShot;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FBattleCameraShot AttackShot;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FBattleCameraShot EnemyShot;
};
