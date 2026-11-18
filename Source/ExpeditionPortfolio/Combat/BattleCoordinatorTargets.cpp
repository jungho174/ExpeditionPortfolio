#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"

UCharacterActionData* UBattleCoordinator::GetTargetSelectionAction() const
{
	return bSelectingTarget && !IsItemCommand(PendingCommand) ? ResolveCommandAction(PendingCommand) : nullptr;
}

bool UBattleCoordinator::IsSelectingAllEnemies() const
{
	const auto* Action=GetTargetSelectionAction();
	return Action && Action->TargetScope==ECharacterActionTargetScope::AllEnemies;
}

TArray<AActor*> UBattleCoordinator::GetSelectedTargets() const
{
	TArray<AActor*> Result;
	if (!bSelectingTarget) { return Result; }
	if (IsSelectingAllEnemies())
	{
		for (const auto& Target:Targets.Candidates)
		{
			if (IsRegisteredEnemy(Target.Get())) { Result.Add(Target.Get()); }
		}
	}
	else if (AActor* Target=Targets.Get()) { Result.Add(Target); }
	return Result;
}

TArray<AActor*> UBattleCoordinator::GetActiveActionTargets() const
{
	TArray<AActor*> Result;
	for (const auto& Target:ActionTargets) { if (Target.IsValid()) { Result.Add(Target.Get()); } }
	if (ActionTargets.IsEmpty() && !IsActiveActionAreaAttack() && CurrentState==EBattleState::PlayerAction && ActiveEnemyActor.IsValid()) { Result.Add(ActiveEnemyActor.Get()); }
	return Result;
}

bool UBattleCoordinator::RefreshAreaPrimaryTarget()
{
	const bool bSelectingArea=IsSelectingAllEnemies();
	const auto* Pending=ResolveCommandAction(PendingCommand);
	const bool bConfirmingArea=bConfirmingTarget && Pending && Pending->TargetScope==ECharacterActionTargetScope::AllEnemies;
	if (!bSelectingArea && !bConfirmingArea && !(CurrentState==EBattleState::PlayerAction && IsActiveActionAreaAttack())) { return false; }
	const auto Candidates=bSelectingArea?GetSelectedTargets():bConfirmingArea?GetBattleEnemies():GetActiveActionTargets();
	for (AActor* Candidate:Candidates)
	{
		if (!IsRegisteredEnemy(Candidate)) { continue; }
		ActiveEnemyActor=Candidate;
		EnemyCombatant=Candidate->FindComponentByClass<UCombatantComponent>();
		if (bSelectingArea) { Targets.Move(0); BroadcastTargetSelection(); }
		return true;
	}
	return false;
}
