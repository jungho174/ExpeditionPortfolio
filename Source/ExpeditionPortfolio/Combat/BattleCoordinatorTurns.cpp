#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

APlayerController* UBattleCoordinator::GetBattlePlayerController() const
{
	return ExplorationPlayerPawn.IsValid() ? Cast<APlayerController>(ExplorationPlayerPawn->GetController()) : nullptr;
}

TArray<AActor*> UBattleCoordinator::GetBattleAllies() const
{
	TArray<AActor*> Actors;
	for (const auto& Member : BattleAllies) { if (Member.IsValid()) { Actors.Add(Member.Get()); } }
	return Actors;
}

TArray<AActor*> UBattleCoordinator::GetBattleEnemies() const
{
	TArray<AActor*> Actors;
	for (const auto& Member : BattleEnemies) { if (Member.IsValid()) { Actors.Add(Member.Get()); } }
	return Actors;
}

AActor* UBattleCoordinator::GetActingCombatant() const
{
	return TurnOrder.IsValidIndex(TurnIndex) ? TurnOrder[TurnIndex].Get() : nullptr;
}

bool UBattleCoordinator::IsRegisteredEnemy(AActor* Actor) const
{
	return FCombatTargetSelection::IsEligible(Actor) && BattleEnemies.Contains(Actor);
}

bool UBattleCoordinator::HasLivingSide(bool bAlly) const
{
	for (const auto& Member : TurnOrder)
	{
		if (BattleAllies.Contains(Member) == bAlly && FCombatTargetSelection::IsEligible(Member.Get())) { return true; }
	}
	return false;
}

bool UBattleCoordinator::SetTurnParticipants(AActor* Actor, bool bAllyTurn)
{
	AActor* Enemy = bAllyTurn ? nullptr : Actor;
	APawn* Ally = bAllyTurn ? Cast<APawn>(Actor) : nullptr;
	if (bAllyTurn)
	{
		for (const auto& Candidate : BattleEnemies)
		{
			if (FCombatTargetSelection::IsEligible(Candidate.Get())) { Enemy=Candidate.Get(); break; }
		}
	}
	else
	{
		TArray<APawn*> Candidates;
		for (const auto& Candidate : TurnOrder)
		{
			if (BattleAllies.Contains(Candidate) && FCombatTargetSelection::IsEligible(Candidate.Get()))
			{
				if (APawn* Pawn=Cast<APawn>(Candidate.Get())) { Candidates.Add(Pawn); }
			}
		}
		if (!Candidates.IsEmpty()) { Ally=Candidates[PatternRandom.RandRange(0,Candidates.Num()-1)]; }
	}
	if (!Ally || !Enemy) { return false; }
	ActivePlayerPawn=Ally; PlayerCombatant=Ally->FindComponentByClass<UCombatantComponent>();
	ActiveEnemyActor=Enemy; EnemyCombatant=Enemy->FindComponentByClass<UCombatantComponent>();
	FBattleFacingRestore::Face(Ally,Enemy); FBattleFacingRestore::Face(Enemy,Ally);
	UE_LOG(LogBattleCoordinator,Log,TEXT("Combatant turn: Round=%d Actor=%s Defender=%s"),BattleRound,*GetNameSafe(Actor),*GetNameSafe(bAllyTurn?Enemy:static_cast<AActor*>(Ally)));
	return true;
}

bool UBattleCoordinator::AdvanceCombatantTurn()
{
	if (TryFinishBattle()) { return true; }
	for (int32 Attempt=0;Attempt<TurnOrder.Num();++Attempt)
	{
		if (++TurnIndex>=TurnOrder.Num()) { TurnIndex=0; ++BattleRound; }
		AActor* Actor=TurnOrder[TurnIndex].Get();
		if (!FCombatTargetSelection::IsEligible(Actor)) { continue; }
		const bool bAllyTurn=BattleAllies.Contains(Actor);
		if (!SetTurnParticipants(Actor,bAllyTurn)) { break; }
		if (bAllyTurn) { return TryTransitionTo(EBattleState::PlayerCommand); }
		SelectEnemyPattern();
		return TryTransitionTo(EBattleState::EnemyTelegraph) && StartEnemyTelegraph();
	}
	return ReturnToExploration();
}

void UBattleCoordinator::HandleParticipantDefeat()
{
	if (TryFinishBattle() || !PlayerCombatant.IsValid() || !EnemyCombatant.IsValid()) { return; }
	if (EnemyCombatant->IsDefeated()) { RefreshAreaPrimaryTarget(); }
	const bool bAllyKO=PlayerCombatant->IsDefeated(),bEnemyKO=EnemyCombatant->IsDefeated();
	if (CurrentState==EBattleState::PlayerCommand && bEnemyKO && !bAllyKO)
	{
		// Cancel a stale confirmation rather than silently redirecting an attack.
		GetWorld()->GetTimerManager().ClearTimer(TargetConfirmationTimer);
		bSelectingTarget=false;bConfirmingTarget=false;Targets.Reset();PendingAction=nullptr;PendingItem=nullptr;
		SetTurnParticipants(GetActingCombatant(),true);BroadcastTargetSelection();return;
	}
	if ((CurrentState==EBattleState::PlayerCommand && bAllyKO)
		|| (CurrentState==EBattleState::PlayerAction && (bAllyKO || (bEnemyKO && !bPlayerHitApplied))))
	{
		GetWorld()->GetTimerManager().ClearTimer(TargetConfirmationTimer);
		bSelectingTarget=false;bConfirmingTarget=false;Targets.Reset();PendingAction=nullptr;PendingItem=nullptr;
		ClearPlayerAction();
		if (TryTransitionTo(EBattleState::TurnEnd)) { ResolveTurnEnd(ECompletedAction::Player); }
		return;
	}
	if ((CurrentState==EBattleState::EnemyTelegraph || CurrentState==EBattleState::EnemyAction || CurrentState==EBattleState::DefenseWindow)
		&& (bAllyKO || (bEnemyKO && !bParryCounterHitApplied)))
	{
		ClearBattleTimers();ClearEnemyAction();FinishEnemyAction();
	}
}
