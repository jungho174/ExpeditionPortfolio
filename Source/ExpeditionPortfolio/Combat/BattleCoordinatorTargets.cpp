#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"
#include "UI/BattlePresentationComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

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
	for (const auto& Target:PlayerAction.Targets) { if (Target.IsValid()) { Result.Add(Target.Get()); } }
	if (PlayerAction.Targets.IsEmpty() && !IsActiveActionAreaAttack() && CurrentState==EBattleState::PlayerAction && ActiveEnemyActor.IsValid()) { Result.Add(ActiveEnemyActor.Get()); }
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

void UBattleCoordinator::BroadcastTargetSelection()
{
	TGuardValue<bool> Guard(bTargetNotification, true);
	TargetSelectionChanged.Broadcast();
}

bool UBattleCoordinator::BeginCommandSelection(EBattlePlayerCommand Command)
{
	if (Command==EBattlePlayerCommand::UsePotion) { return BeginItemSelection(GetDefaultItem()); }
	if (bTargetNotification || bSelectingTarget || IsBattleOperationBusy() || !CanRequestPlayerCommand(Command)) { return false; }
	// Capture only this encounter's living opponents, before the target camera moves.
	FVector Forward = (ActiveEnemyActor->GetActorLocation() - ActivePlayerPawn->GetActorLocation()).GetSafeNormal2D();
	TArray<AActor*> Candidates;
	if (IsItemCommand(Command))
	{
		if (CanHealAlly(ActivePlayerPawn.Get())) { Candidates.Add(ActivePlayerPawn.Get()); }
		for (const auto& Ally:BattleAllies) { if (CanHealAlly(Ally.Get())) { Candidates.AddUnique(Ally.Get()); } }
	}
	else { for (const auto& Enemy:BattleEnemies) { Candidates.Add(Enemy.Get()); } }
	Targets.Build(Candidates, FVector::CrossProduct(FVector::UpVector, Forward));
	if (!Targets.Get()) { return false; }
	if (Command!=EBattlePlayerCommand::CharacterAction) { PendingAction=nullptr; }
	PendingCommand = Command;
	bSelectingTarget = true;
	BroadcastTargetSelection();
	return true;
}

bool UBattleCoordinator::IsRegisteredAlly(AActor* Ally) const
{
	return IsValid(Ally) && Ally->GetWorld()==GetWorld() && (Ally==ActivePlayerPawn.Get() || BattleAllies.Contains(Ally));
}

bool UBattleCoordinator::CanHealAlly(AActor* Ally) const
{
	if (!IsRegisteredAlly(Ally)) { return false; }
	const auto* Stats=Ally->FindComponentByClass<UCombatantComponent>();
	return Stats && !Stats->IsDefeated();
}

bool UBattleCoordinator::RegisterBattleAlly(AActor* Ally)
{
	if (CurrentState!=EBattleState::PlayerCommand || bSelectingTarget || bConfirmingTarget || bTargetNotification
		|| !HasValidActiveBattle() || !FCombatTargetSelection::IsEligible(Ally) || Ally==ActiveEnemyActor.Get() || Ally->GetWorld()!=GetWorld()) { return false; }
	const auto* Stats=Ally->FindComponentByClass<UCombatantComponent>();
	if (!IsContentFreeTest() && (!Stats || Stats->Persistence!=ECombatantPersistence::UniquePartyMember || !Stats->HasValidIdentity())) { return false; }
	BattleAllies.AddUnique(Ally); CaptureParticipant(Ally, true);
	if (Cast<APawn>(Ally) && !TurnOrder.Contains(Ally)) { TurnOrder.Insert(Ally,BattleAllies.Num()-1); }
	return true;
}

bool UBattleCoordinator::MoveTargetSelection(int32 Direction)
{
	if (!bSelectingTarget || bTargetNotification || IsBattleOperationBusy()) { return false; }
	if (IsSelectingAllEnemies()) { return false; }
	if (!HasValidActiveBattle()) { CancelTargetSelection(); ReturnToExploration(); return false; }
	if (IsItemCommand(PendingCommand))
	{
		Targets.Candidates.RemoveAll([this](const TWeakObjectPtr<AActor>& Ally) { return !CanHealAlly(Ally.Get()); });
	}
	Targets.Move(Direction);
	BroadcastTargetSelection();
	if (Direction && Targets.Get()) { TGuardValue<bool> Guard(bTargetNotification,true); TargetNavigation.Broadcast(); }
	return Targets.Get() != nullptr;
}

bool UBattleCoordinator::CancelTargetSelection()
{
	if (!bSelectingTarget || bTargetNotification || IsBattleOperationBusy()) { return false; }
	bSelectingTarget = false;
	PendingAction=nullptr;PendingItem=nullptr;
	Targets.Reset();
	BroadcastTargetSelection();
	return true;
}

bool UBattleCoordinator::ConfirmTargetSelection()
{
	if (!bSelectingTarget || bTargetNotification || IsBattleOperationBusy()) { return false; }
	if (IsSelectingAllEnemies()) { Targets.Move(0); RefreshAreaPrimaryTarget(); }
	const bool bPotion=IsItemCommand(PendingCommand);
	if (!HasValidActiveBattle() || (bPotion ? !CanHealAlly(Targets.Get()) : !IsRegisteredEnemy(Targets.Get())) || !CanRequestPlayerCommand(PendingCommand))
	{
		CancelTargetSelection();
		ReturnToExploration();
		return false;
	}
	ItemTarget=bPotion?Targets.Get():nullptr;
	if (!bPotion) { ActiveEnemyActor=Targets.Get(); EnemyCombatant=Targets.Get()->FindComponentByClass<UCombatantComponent>(); }
	bConfirmedAlly=bPotion;
	bSelectingTarget = false;
	Targets.Reset();
	bConfirmingTarget = true;
	{ TGuardValue<bool> Guard(bTargetNotification,true); TargetConfirmed.Broadcast(); }
	// 선택 확정을 먼저 알리고, 카메라가 어깨 시점으로 돌아올 때까지 새 명령을 막는다.
	BroadcastTargetSelection();
	const float Delay = BattlePresentation.IsValid() ? BattlePresentation->GetTargetConfirmationDuration() : 0.47f;
	GetWorld()->GetTimerManager().SetTimer(TargetConfirmationTimer,
		FTimerDelegate::CreateUObject(this, &ThisClass::FinishTargetConfirmation, BattleSerial), Delay, false);
	return true;
}

void UBattleCoordinator::FinishTargetConfirmation(int64 ExpectedBattleId)
{
	if (!bConfirmingTarget || ExpectedBattleId != BattleSerial || CurrentState != EBattleState::PlayerCommand) { return; }
	bConfirmingTarget = false;
	if (!HasValidActiveBattle()) { ReturnToExploration(); BroadcastTargetSelection(); return; }
	if (IsItemCommand(PendingCommand) && !CanHealAlly(ItemTarget.Get())) { ItemTarget.Reset(); BroadcastTargetSelection(); return; }
	if (PendingCommand==EBattlePlayerCommand::CharacterAction) { RequestPlayerAction(PendingAction); }
	else { RequestPlayerCommand(PendingCommand); }
	PendingAction=nullptr;
	BroadcastTargetSelection();
}
