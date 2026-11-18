#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/QTEPresentationData.h"
#include "BattleUIAudioData.generated.h"

UENUM(BlueprintType)
enum class EBattleUIAudio : uint8 { ItemMenu, SkillMenu, NoUses, AllyTargetStart, AllyTargetMove, AllyTurn, EnemyTurn };

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UBattleUIAudioData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu") FQTESound ItemMenu;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu") FQTESound SkillMenu;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Menu") FQTESound NoUses;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Target") FQTESound AllyTargetStart;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Target") FQTESound AllyTargetMove;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Turn") FQTESound AllyTurn;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Turn") FQTESound EnemyTurn;
	const FQTESound& Get(EBattleUIAudio Cue) const
	{
		switch(Cue)
		{
		case EBattleUIAudio::ItemMenu:return ItemMenu;
		case EBattleUIAudio::SkillMenu:return SkillMenu;
		case EBattleUIAudio::NoUses:return NoUses;
		case EBattleUIAudio::AllyTargetStart:return AllyTargetStart;
		case EBattleUIAudio::AllyTargetMove:return AllyTargetMove;
		case EBattleUIAudio::AllyTurn:return AllyTurn;
		default:return EnemyTurn;
		}
	}
};
