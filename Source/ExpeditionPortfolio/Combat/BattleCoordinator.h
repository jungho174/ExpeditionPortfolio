// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/BattleState.h"
#include "Subsystems/WorldSubsystem.h"
#include "BattleCoordinator.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBattleCoordinator, Log, All);

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnBattleStateChanged,
	EBattleState /* PreviousState */,
	EBattleState /* NewState */);

UCLASS()
class EXPEDITIONPORTFOLIO_API UBattleCoordinator : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	EBattleState GetCurrentState() const { return CurrentState; }
	bool TryTransitionTo(EBattleState NewState);

	FOnBattleStateChanged& OnBattleStateChanged() { return BattleStateChanged; }

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	bool IsTransitionAllowed(EBattleState NewState) const;

	EBattleState CurrentState = EBattleState::Exploration;
	FOnBattleStateChanged BattleStateChanged;
};
