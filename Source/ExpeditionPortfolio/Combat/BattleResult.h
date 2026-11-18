#pragma once

#include "CoreMinimal.h"
#include "BattleResult.generated.h"

USTRUCT(BlueprintType)
struct FBattlePartyResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FText Name;
	UPROPERTY(BlueprintReadOnly) float Health = 0;
	UPROPERTY(BlueprintReadOnly) float MaxHealth = 1;
};

USTRUCT(BlueprintType)
struct FBattleResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bVictory = false;
	UPROPERTY(BlueprintReadOnly) int32 SuccessfulDodges = 0;
	UPROPERTY(BlueprintReadOnly) int32 SuccessfulParries = 0;
	UPROPERTY(BlueprintReadOnly) float DamageDealt = 0;
	UPROPERTY(BlueprintReadOnly) float DamageReceived = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FBattlePartyResult> Party;
};
