// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ExpeditionPortfolioGameMode.generated.h"
class UBattleRulesData;

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class AExpeditionPortfolioGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Combat|Rules") TObjectPtr<UBattleRulesData> BattleRules;
	
	/** Constructor */
	AExpeditionPortfolioGameMode();
};



