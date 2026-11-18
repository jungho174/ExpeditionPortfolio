// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BattlePlayerCommand.generated.h"

UENUM(BlueprintType)
enum class EBattlePlayerCommand : uint8
{
	BasicAttack,
	PowerStrike,
	MeleeAttack,
	UsePotion,
	CharacterAction,
	UseItem
};
