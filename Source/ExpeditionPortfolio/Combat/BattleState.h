// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BattleState.generated.h"

UENUM(BlueprintType)
enum class EBattleState : uint8
{
	Exploration,
	BattleIntro,
	PlayerCommand,
	PlayerAction,
	EnemyTelegraph,
	EnemyAction,
	DefenseWindow,
	TurnEnd,
	Victory,
	Defeat,
	ReturnToField
};
