#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PlayerCommandData.generated.h"

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UPlayerCommandData : public UDataAsset
{
	GENERATED_BODY()
public:
	// Read old packages only. Active basic-attack damage belongs to CharacterActions.BasicAttack.
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	float MeleeDamage = 15.0f;
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	float PotionHealing = 30.0f;
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	int32 PotionsPerEncounter = 3;
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	float TurnTransitionDelay = 1.0f;
	bool IsValidConfiguration() const
	{
		return FMath::IsFinite(MeleeDamage) && MeleeDamage > 0 && FMath::IsFinite(PotionHealing)
			&& PotionHealing > 0 && PotionsPerEncounter >= 0 && PotionsPerEncounter <= 99
			&& FMath::IsFinite(TurnTransitionDelay) && TurnTransitionDelay>=0 && TurnTransitionDelay<=5;
	}
};
