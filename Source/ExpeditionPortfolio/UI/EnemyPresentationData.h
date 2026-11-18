#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/QTEPresentationData.h"
#include "Combat/BattleGrounding.h"
#include "EnemyPresentationData.generated.h"
class UAnimMontage;
class UMaterialInterface;
UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UEnemyPresentationData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Movement") bool bApproachTarget=false;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Movement",meta=(ClampMin="0.05",Units="s")) float ApproachDuration=.6f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Movement",meta=(ClampMin="0.05",Units="s")) float ReturnDuration=.65f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Movement",meta=(ClampMin="10",Units="cm")) float TargetDistance=200;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Movement",meta=(ClampMin="10",Units="cm")) float PlayerMeleeDistance=200;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Movement") TObjectPtr<UAnimMontage> WalkMontage;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Movement") FQTESound ApproachSound;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Movement") FQTESound ReturnSound;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Movement|Grounding") FBattleGroundingSettings Grounding;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Reaction") TObjectPtr<UAnimMontage> HitReaction;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Reaction") TObjectPtr<UAnimMontage> Death;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Reaction|Death",meta=(ClampMin="0.05")) float DeathPlayRate=1.f;
	// Game time after the death montage ends. The final pose is held until disappearance.
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Reaction|Death",meta=(ClampMin="0",Units="s")) float DeathDisappearDelay=.2f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Reaction|Death",meta=(ClampMin="0",Units="s")) float DeathFadeDuration=.8f;
	// Optional fade-capable replacements, indexed by skeletal-mesh material slot. Originals are restored on retry.
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Reaction|Death") TArray<TObjectPtr<UMaterialInterface>> DeathFadeMaterials;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Reaction|Death") FName DeathFadeParameter=TEXT("DeathOpacity");
	// Still remove an enemy whose optional death montage is missing or cannot play.
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Reaction|Death",meta=(ClampMin="0.05",Units="s")) float DeathFallbackDuration=.75f;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Reaction") FQTESound HitSound;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Reaction") FQTESound DeathSound;
};
