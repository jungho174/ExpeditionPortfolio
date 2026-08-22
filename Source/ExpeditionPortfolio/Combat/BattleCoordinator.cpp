// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BattleCoordinator.h"

#include "Combat/CombatantComponent.h"
#include "GameFramework/Pawn.h"
#include "UObject/EnumProperty.h"

DEFINE_LOG_CATEGORY(LogBattleCoordinator);

void UBattleCoordinator::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentState = EBattleState::Exploration;
	ActiveEncounterSource.Reset();
	ActivePlayerPawn.Reset();
	ActiveEnemyActor.Reset();
	PlayerCombatant.Reset();
	EnemyCombatant.Reset();
}

void UBattleCoordinator::Deinitialize()
{
	BattleStateChanged.Clear();
	ActiveEncounterSource.Reset();
	ActivePlayerPawn.Reset();
	ActiveEnemyActor.Reset();
	PlayerCombatant.Reset();
	EnemyCombatant.Reset();
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

bool UBattleCoordinator::RequestBattleStart(AActor* EncounterSource, APawn* PlayerPawn, AActor* EnemyActor)
{
	if (!IsValid(EncounterSource) || !IsValid(PlayerPawn) || !IsValid(EnemyActor))
	{
		UE_LOG(
			LogBattleCoordinator,
			Warning,
			TEXT("Rejected battle start request: EncounterSource, PlayerPawn, and EnemyActor must be valid."));
		return false;
	}

	if (PlayerPawn == EnemyActor)
	{
		UE_LOG(
			LogBattleCoordinator,
			Warning,
			TEXT("Rejected battle start request: PlayerPawn and EnemyActor must be different actors."));
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

	if (ActiveEncounterSource.IsValid()
		|| ActivePlayerPawn.IsValid()
		|| ActiveEnemyActor.IsValid()
		|| PlayerCombatant.IsValid()
		|| EnemyCombatant.IsValid())
	{
		UE_LOG(
			LogBattleCoordinator,
			Warning,
			TEXT("Rejected battle start request: another encounter is already active."));
		return false;
	}

	UCombatantComponent* FoundPlayerCombatant = PlayerPawn->FindComponentByClass<UCombatantComponent>();
	UCombatantComponent* FoundEnemyCombatant = EnemyActor->FindComponentByClass<UCombatantComponent>();
	if (!IsValid(FoundPlayerCombatant) || !IsValid(FoundEnemyCombatant))
	{
		UE_LOG(
			LogBattleCoordinator,
			Warning,
			TEXT("Rejected battle start request: Player '%s' and Enemy '%s' must each have a CombatantComponent."),
			*GetNameSafe(PlayerPawn),
			*GetNameSafe(EnemyActor));
		return false;
	}

	if (FoundPlayerCombatant == FoundEnemyCombatant)
	{
		UE_LOG(
			LogBattleCoordinator,
			Warning,
			TEXT("Rejected battle start request: Player and Enemy must use different CombatantComponents."));
		return false;
	}

	ActiveEncounterSource = EncounterSource;
	ActivePlayerPawn = PlayerPawn;
	ActiveEnemyActor = EnemyActor;
	PlayerCombatant = FoundPlayerCombatant;
	EnemyCombatant = FoundEnemyCombatant;

	if (!TryTransitionTo(EBattleState::BattleIntro))
	{
		ActiveEncounterSource.Reset();
		ActivePlayerPawn.Reset();
		ActiveEnemyActor.Reset();
		PlayerCombatant.Reset();
		EnemyCombatant.Reset();
		return false;
	}

	UE_LOG(
		LogBattleCoordinator,
		Log,
		TEXT("Battle participants registered: Player='%s', Enemy='%s'."),
		*GetNameSafe(PlayerPawn),
		*GetNameSafe(EnemyActor));

	// BattleIntro has no asynchronous setup or presentation yet, so complete it immediately.
	if (!TryTransitionTo(EBattleState::PlayerCommand))
	{
		UE_LOG(
			LogBattleCoordinator,
			Error,
			TEXT("Battle start entered BattleIntro but could not continue to PlayerCommand."));
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
