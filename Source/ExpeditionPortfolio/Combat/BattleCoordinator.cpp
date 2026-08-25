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
	ClearBattleContext();
	bTransitionInProgress = false;
	bActionInProgress = false;
}

void UBattleCoordinator::Deinitialize()
{
	BattleStateChanged.Clear();
	BattleStateChangedBlueprint.Clear();
	ClearBattleContext();
	bTransitionInProgress = false;
	bActionInProgress = false;
	Super::Deinitialize();
}

bool UBattleCoordinator::TryTransitionTo(const EBattleState NewState)
{
	if (bTransitionInProgress)
	{
		UE_LOG(
			LogBattleCoordinator,
			Warning,
			TEXT("Rejected reentrant battle state transition: %s -> %s"),
			*UEnum::GetValueAsString(CurrentState),
			*UEnum::GetValueAsString(NewState));
		return false;
	}

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
	TGuardValue<bool> TransitionGuard(bTransitionInProgress, true);

	UE_LOG(
		LogBattleCoordinator,
		Log,
		TEXT("Battle state changed: %s -> %s"),
		*UEnum::GetValueAsString(PreviousState),
		*UEnum::GetValueAsString(CurrentState));

	BattleStateChanged.Broadcast(PreviousState, CurrentState);
	BattleStateChangedBlueprint.Broadcast(PreviousState, CurrentState);
	return true;
}

bool UBattleCoordinator::RequestBattleStart(AActor* EncounterSource, APawn* PlayerPawn, AActor* EnemyActor)
{
	if (bTransitionInProgress || bActionInProgress)
	{
		LogRejectedCommand(TEXT("RequestBattleStart"), TEXT("another battle operation is in progress"));
		return false;
	}

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
	if (FoundPlayerCombatant->IsDefeated() || FoundEnemyCombatant->IsDefeated())
	{
		UE_LOG(
			LogBattleCoordinator,
			Warning,
			TEXT("Rejected battle start request: Player and Enemy must both be able to act."));
		return false;
	}


	ActiveEncounterSource = EncounterSource;
	ActivePlayerPawn = PlayerPawn;
	ActiveEnemyActor = EnemyActor;
	PlayerCombatant = FoundPlayerCombatant;
	EnemyCombatant = FoundEnemyCombatant;
	bPowerStrikeUsed = false;

	if (!TryTransitionTo(EBattleState::BattleIntro))
	{
		ClearBattleContext();
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
		ReturnToExploration();
		return false;
	}

	return true;
}

bool UBattleCoordinator::RequestPlayerBasicAttack()
{
	return RequestPlayerCommand(EBattlePlayerCommand::BasicAttack);
}

bool UBattleCoordinator::RequestPlayerPowerStrike()
{
	return RequestPlayerCommand(EBattlePlayerCommand::PowerStrike);
}

bool UBattleCoordinator::CanRequestPlayerCommand(const EBattlePlayerCommand Command) const
{
	if (CurrentState != EBattleState::PlayerCommand
		|| !HasValidActiveBattle())
	{
		return false;
	}

	switch (Command)
	{
	case EBattlePlayerCommand::BasicAttack:
		return true;

	case EBattlePlayerCommand::PowerStrike:
		return !bPowerStrikeUsed;

	default:
		return false;
	}
}

bool UBattleCoordinator::RequestPlayerCommand(const EBattlePlayerCommand Command)
{
	if (bTransitionInProgress || bActionInProgress)
	{
		LogRejectedCommand(TEXT("RequestPlayerCommand"), TEXT("another battle operation is in progress"));
		return false;
	}

	if (CurrentState != EBattleState::PlayerCommand)
	{
		LogRejectedCommand(TEXT("RequestPlayerCommand"), TEXT("current state is not PlayerCommand"));
		return false;
	}

	if (!HasValidActiveBattle())
	{
		LogRejectedCommand(TEXT("RequestPlayerCommand"), TEXT("active battle participants are invalid or defeated"));
		ReturnToExploration();
		return false;
	}

	if (Command == EBattlePlayerCommand::PowerStrike && bPowerStrikeUsed)
	{
		LogRejectedCommand(TEXT("RequestPlayerCommand"), TEXT("PowerStrike has already been used in this battle"));
		return false;
	}

	if (Command != EBattlePlayerCommand::BasicAttack && Command != EBattlePlayerCommand::PowerStrike)
	{
		LogRejectedCommand(TEXT("RequestPlayerCommand"), TEXT("the requested command is not supported"));
		return false;
	}

	return ExecutePlayerCommand(Command);
}

bool UBattleCoordinator::ExecutePlayerCommand(const EBattlePlayerCommand Command)
{
	const float Damage = Command == EBattlePlayerCommand::PowerStrike
		? PlayerPowerStrikeDamage
		: PlayerBasicAttackDamage;

	if (Command == EBattlePlayerCommand::PowerStrike)
	{
		bPowerStrikeUsed = true;
	}

	TGuardValue<bool> ActionGuard(bActionInProgress, true);

	if (!TryTransitionTo(EBattleState::PlayerAction))
	{
		return false;
	}

	if (!EnemyCombatant->ApplyDamage(Damage))
	{
		LogRejectedCommand(TEXT("RequestPlayerCommand"), TEXT("player damage could not be applied"));
		ReturnToExploration();
		return false;
	}

	if (!TryTransitionTo(EBattleState::TurnEnd))
	{
		ReturnToExploration();
		return false;
	}

	return ResolveTurnEnd(ECompletedAction::Player);
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
		return NewState == EBattleState::PlayerCommand
			|| NewState == EBattleState::ReturnToField;

	case EBattleState::PlayerCommand:
		return NewState == EBattleState::PlayerAction
			|| NewState == EBattleState::ReturnToField;

	case EBattleState::PlayerAction:
		return NewState == EBattleState::TurnEnd
			|| NewState == EBattleState::ReturnToField;

	case EBattleState::EnemyTelegraph:
		return NewState == EBattleState::EnemyAction
			|| NewState == EBattleState::ReturnToField;

	case EBattleState::EnemyAction:
		return NewState == EBattleState::TurnEnd
			|| NewState == EBattleState::ReturnToField;

	case EBattleState::TurnEnd:
		return NewState == EBattleState::Victory
			|| NewState == EBattleState::Defeat
			|| NewState == EBattleState::PlayerCommand
			|| NewState == EBattleState::EnemyTelegraph
			|| NewState == EBattleState::ReturnToField;

	case EBattleState::Victory:
	case EBattleState::Defeat:
		return NewState == EBattleState::ReturnToField;

	case EBattleState::ReturnToField:
		return NewState == EBattleState::Exploration;

	default:
		return false;
	}
}

bool UBattleCoordinator::HasValidActiveBattle() const
{
	return ActiveEncounterSource.IsValid()
		&& ActivePlayerPawn.IsValid()
		&& ActiveEnemyActor.IsValid()
		&& PlayerCombatant.IsValid()
		&& EnemyCombatant.IsValid()
		&& !PlayerCombatant->IsDefeated()
		&& !EnemyCombatant->IsDefeated();
}

bool UBattleCoordinator::ExecuteEnemyTurn()
{
	if (!HasValidActiveBattle())
	{
		ReturnToExploration();
		return false;
	}

	if (!TryTransitionTo(EBattleState::EnemyAction))
	{
		ReturnToExploration();
		return false;
	}

	if (!PlayerCombatant->ApplyDamage(EnemyBasicAttackDamage))
	{
		ReturnToExploration();
		return false;
	}

	if (!TryTransitionTo(EBattleState::TurnEnd))
	{
		ReturnToExploration();
		return false;
	}

	return ResolveTurnEnd(ECompletedAction::Enemy);
}

bool UBattleCoordinator::ResolveTurnEnd(const ECompletedAction CompletedAction)
{
	if (CurrentState != EBattleState::TurnEnd)
	{
		return false;
	}

	if (!ActivePlayerPawn.IsValid()
		|| !ActiveEnemyActor.IsValid()
		|| !PlayerCombatant.IsValid()
		|| !EnemyCombatant.IsValid())
	{
		ReturnToExploration();
		return false;
	}

	if (PlayerCombatant->IsDefeated())
	{
		return TryTransitionTo(EBattleState::Defeat);
	}

	if (EnemyCombatant->IsDefeated())
	{
		if (!TryTransitionTo(EBattleState::Victory))
		{
			return false;
		}

		return ReturnToExploration();
	}

	if (CompletedAction == ECompletedAction::Player)
	{
		if (!TryTransitionTo(EBattleState::EnemyTelegraph))
		{
			return false;
		}

		return ExecuteEnemyTurn();
	}

	return TryTransitionTo(EBattleState::PlayerCommand);
}

bool UBattleCoordinator::ReturnToExploration()
{
	if (CurrentState == EBattleState::Exploration)
	{
		ClearBattleContext();
		return true;
	}

	if (CurrentState != EBattleState::ReturnToField
		&& !TryTransitionTo(EBattleState::ReturnToField))
	{
		UE_LOG(
			LogBattleCoordinator,
			Error,
			TEXT("Could not enter ReturnToField from %s."),
			*UEnum::GetValueAsString(CurrentState));
		return false;
	}

	ClearBattleContext();
	return TryTransitionTo(EBattleState::Exploration);
}

void UBattleCoordinator::ClearBattleContext()
{
	ActiveEncounterSource.Reset();
	ActivePlayerPawn.Reset();
	ActiveEnemyActor.Reset();
	PlayerCombatant.Reset();
	EnemyCombatant.Reset();
	bPowerStrikeUsed = false;
}

void UBattleCoordinator::LogRejectedCommand(const TCHAR* CommandName, const TCHAR* Reason) const
{
	UE_LOG(
		LogBattleCoordinator,
		Warning,
		TEXT("Rejected battle command '%s': %s."),
		CommandName,
		Reason);
}
