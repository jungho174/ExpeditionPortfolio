// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BattleCoordinator.h"

#include "GameFramework/Pawn.h"
#include "UObject/EnumProperty.h"

DEFINE_LOG_CATEGORY(LogBattleCoordinator);

void UBattleCoordinator::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentState = EBattleState::Exploration;
	ActiveEncounterSource.Reset();
	ActivePlayerPawn.Reset();
}

void UBattleCoordinator::Deinitialize()
{
	BattleStateChanged.Clear();
	ActiveEncounterSource.Reset();
	ActivePlayerPawn.Reset();
	Super::Deinitialize();
}

bool UBattleCoordinator::TryTransitionTo(const EBattleState NewState)
{
	if (!IsTransitionAllowed(NewState))
	{
		UE_LOG(
			LogBattleCoordinator,
			Warning,
			TEXT("Rejected battle state transition: %s -> %s"),
			*UEnum::GetValueAsString(CurrentState),
			*UEnum::GetValueAsString(NewState));
		return false;
	}

	const EBattleState PreviousState = CurrentState;
	CurrentState = NewState;

	UE_LOG(
		LogBattleCoordinator,
		Log,
		TEXT("Battle state changed: %s -> %s"),
		*UEnum::GetValueAsString(PreviousState),
		*UEnum::GetValueAsString(CurrentState));

	BattleStateChanged.Broadcast(PreviousState, CurrentState);
	return true;
}

bool UBattleCoordinator::RequestBattleStart(AActor* EncounterSource, APawn* PlayerPawn)
{
	if (!IsValid(EncounterSource) || !IsValid(PlayerPawn))
	{
		UE_LOG(
			LogBattleCoordinator,
			Warning,
			TEXT("Rejected battle start request: EncounterSource and PlayerPawn must be valid."));
		return false;
	}

	if (CurrentState != EBattleState::Exploration)
	{
		UE_LOG(
			LogBattleCoordinator,
			Warning,
			TEXT("Rejected battle start request: current state is %s, expected Exploration."),
			*UEnum::GetValueAsString(CurrentState));
		return false;
	}

	if (ActiveEncounterSource.IsValid() || ActivePlayerPawn.IsValid())
	{
		UE_LOG(
			LogBattleCoordinator,
			Warning,
			TEXT("Rejected battle start request: another encounter is already active."));
		return false;
	}

	ActiveEncounterSource = EncounterSource;
	ActivePlayerPawn = PlayerPawn;

	if (!TryTransitionTo(EBattleState::BattleIntro))
	{
		ActiveEncounterSource.Reset();
		ActivePlayerPawn.Reset();
		return false;
	}

	return true;
}

bool UBattleCoordinator::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UBattleCoordinator::IsTransitionAllowed(const EBattleState NewState) const
{
	if (NewState == CurrentState)
	{
		return false;
	}

	switch (CurrentState)
	{
	case EBattleState::Exploration:
		return NewState == EBattleState::BattleIntro;

	case EBattleState::BattleIntro:
		return NewState == EBattleState::PlayerCommand;

	default:
		return false;
	}
}
