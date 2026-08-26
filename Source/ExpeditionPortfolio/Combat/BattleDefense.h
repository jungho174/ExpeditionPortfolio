// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BattleDefense.generated.h"

UENUM(BlueprintType)
enum class EBattleDefenseCommand : uint8
{
	Parry,
	Dodge
};

UENUM(BlueprintType)
enum class EBattleDefenseResult : uint8
{
	None,
	ParrySuccess,
	DodgeSuccess,
	Failed,
	NoInput
};
