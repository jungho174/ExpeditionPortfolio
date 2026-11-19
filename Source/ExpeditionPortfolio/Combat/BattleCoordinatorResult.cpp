// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BattleCoordinator.h"
#include "Combat/ActionQTESubsystem.h"
#include "UI/BattleResultPresentationComponent.h"
#include "Exploration/Encounters/BattleEncounterTrigger.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void FBattleEncounterRecord::BindParticipant(AActor* Actor, bool bAlly, const FOnCombatantHealthChanged::FDelegate& OnChange)
{
	if (!Actor || Entry.Participants.ContainsByPredicate([Actor](const FBattleParticipantEntry& Member) { return Member.Actor.Get() == Actor; })) { return; }
	auto* Combatant = Actor->FindComponentByClass<UCombatantComponent>();
	if (!Combatant) { return; }

	FBattleParticipantEntry& Member = Entry.Participants.AddDefaulted_GetRef();
	Member.Actor = Actor;
	Member.Combatant = Combatant;
	Member.bAlly = bAlly;
	Member.Health = Combatant->GetCurrentHealth();
	Member.Transform = Actor->GetActorTransform();
	Member.FacingRestore.Capture(Actor);
	Member.HealthHandle = Combatant->OnHealthChanged().Add(OnChange);
}

void FBattleEncounterRecord::ReleaseParticipants()
{
	for (auto& Member : Entry.Participants)
	{
		Member.FacingRestore.Restore();
		if (Member.Combatant.IsValid()) { Member.Combatant->OnHealthChanged().Remove(Member.HealthHandle); }
	}
	Entry.Participants.Reset();
}

void FBattleEncounterRecord::RecordHealthChange(bool bAlly, float Before, float After)
{
	// 실제로 감소한 체력만 집계해 과잉 피해와 회복량이 피해 통계에 섞이지 않게 한다.
	const float Damage = FMath::Max(0.0f, Before - After);
	if (bAlly) { Result.DamageReceived += Damage; }
	else { Result.DamageDealt += Damage; }
}

void FBattleEncounterRecord::RecordDefense(EBattleDefenseResult Defense)
{
	if (Defense == EBattleDefenseResult::ParrySuccess) { ++Result.SuccessfulParries; }
	if (Defense == EBattleDefenseResult::DodgeSuccess) { ++Result.SuccessfulDodges; }
}

void FBattleEncounterRecord::CaptureOutcome(bool bVictory, const AActor* ActingAlly)
{
	Result.bVictory = bVictory;
	Result.Party.Reset();
	for (const auto& Member : Entry.Participants)
	{
		if (!Member.bAlly || !Member.Combatant.IsValid()) { continue; }
		FBattlePartyResult& Ally = Result.Party.AddDefaulted_GetRef();
		Ally.Name = Member.Combatant->CombatDisplayName;
		if (Ally.Name.IsEmpty() || Ally.Name.ToString() == TEXT("적"))
		{
			Ally.Name = FText::FromString(Member.Actor.Get() == ActingAlly ? TEXT("플레이어") : TEXT("동료"));
		}
		Ally.Health = Member.Combatant->GetCurrentHealth();
		Ally.MaxHealth = Member.Combatant->GetMaxHealth();
	}
}

void FBattleEncounterEntry::RestoreParticipants() const
{
	for (const auto& Member : Participants)
	{
		if (Member.Actor.IsValid()) { Member.Actor->SetActorTransform(Member.Transform, false, nullptr, ETeleportType::TeleportPhysics); }
		if (Member.Combatant.IsValid()) { Member.Combatant->RestoreBattleHealth(Member.Health); }
	}
}

void UBattleCoordinator::CaptureParticipant(AActor* Actor, bool bAlly)
{
	EncounterRecord.BindParticipant(Actor, bAlly, FOnCombatantHealthChanged::FDelegate::CreateUObject(
		this, &ThisClass::HandleParticipantHealthChanged, bAlly));
}

void UBattleCoordinator::HandleParticipantHealthChanged(float Before, float After, bool bAlly)
{
	if (CurrentState == EBattleState::Victory || CurrentState == EBattleState::Defeat) { return; }
	EncounterRecord.RecordHealthChange(bAlly, Before, After);
	if (After > 0 || ResultCheckTicker.IsValid()) { return; }

	// 외부 피해는 체력 통지가 끝난 뒤 처리해, 같은 타격 도중 턴이 바뀌지 않게 한다.
	ResultCheckTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		if (IsBattleOperationBusy()) { return true; }
		ResultCheckTicker.Reset();
		HandleParticipantDefeat();
		return false;
	}));
}

void UBattleCoordinator::ClearParticipants()
{
	if (ResultCheckTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ResultCheckTicker);
		ResultCheckTicker.Reset();
	}
	EncounterRecord.ReleaseParticipants();
}

// 한 진영이 전멸하면 결과를 고정하고, 남은 행동 타이머를 취소한 뒤 결과를 알린다.
bool UBattleCoordinator::TryFinishBattle()
{
	if (CurrentState==EBattleState::Victory || CurrentState==EBattleState::Defeat) { return true; }
	if (CurrentState==EBattleState::Exploration || CurrentState==EBattleState::ReturnToField
		|| !PlayerCombatant.IsValid() || !EnemyCombatant.IsValid()) { return false; }
	const bool bAllEnemiesDefeated=!HasLivingSide(false);
	const bool bDefeat=!HasLivingSide(true);
	if (!bDefeat && !bAllEnemiesDefeated) { return false; }
	EncounterRecord.CaptureOutcome(!bDefeat, ActivePlayerPawn.Get());
	ClearBattleTimers();
	auto& Timers=GetWorld()->GetTimerManager();
	Timers.ClearTimer(IntroTimer); Timers.ClearTimer(TargetConfirmationTimer); Timers.ClearTimer(PlayerAction.TimeoutTimer);
	EnemyAction.ClearTimers(Timers);
	GetWorld()->GetSubsystem<UActionQTESubsystem>()->CancelQTE(PlayerAction.QTESessionId);
	bSelectingTarget=false; bConfirmingTarget=false;
	if (!TryTransitionTo(bDefeat?EBattleState::Defeat:EBattleState::Victory)) { return false; }
	// No local presentation (automation/server) still exposes the same explicit result decisions.
	if (!GetBattlePlayerController() || !GetBattlePlayerController()->FindComponentByClass<UBattleResultPresentationComponent>())
	{
		NotifyResultReady();
	}
	const FBattleResult& Result = EncounterRecord.Result;
	UE_LOG(LogBattleCoordinator,Log,TEXT("Battle result captured: Victory=%d Dealt=%.1f Received=%.1f Parries=%d Dodges=%d"),
		Result.bVictory, Result.DamageDealt, Result.DamageReceived, Result.SuccessfulParries, Result.SuccessfulDodges);
	return true;
}

void UBattleCoordinator::NotifyResultReady()
{
	if (CurrentState==EBattleState::Victory || CurrentState==EBattleState::Defeat) { EncounterRecord.bReady=true; }
}

bool UBattleCoordinator::ContinueAfterVictory()
{
	if (!EncounterRecord.bReady || IsBattleOperationBusy() || CurrentState!=EBattleState::Victory) { return false; }
	TGuardValue<bool> ActionGuard(bActionInProgress,true);
	TWeakObjectPtr<ABattleEncounterTrigger> Trigger=Cast<ABattleEncounterTrigger>(ActiveEncounterSource.Get());
	TArray<TWeakObjectPtr<AActor>> DefeatedEnemies;
	for (const auto& Member : EncounterRecord.Entry.Participants)
	{
		if (!Member.bAlly && Member.Actor.IsValid()) { DefeatedEnemies.AddUnique(Member.Actor); }
	}
	if (!TryTransitionTo(EBattleState::ReturnToField)) { return false; }
	// Unbind combat callbacks, stop actions and restore camera/world while the actors still exist.
	ClearBattleContext();
	if (Trigger.IsValid()) { Trigger->CompleteEncounter(); }
	for (const TWeakObjectPtr<AActor>& Enemy:DefeatedEnemies)
	{
		if (Enemy.IsValid())
		{
			Enemy->SetActorEnableCollision(false);
			Enemy->Destroy();
		}
	}
	return TryTransitionTo(EBattleState::Exploration);
}

bool UBattleCoordinator::RetryBattle() { return RestoreEncounter(true); }

bool UBattleCoordinator::RetreatFromBattle() { return RestoreEncounter(false); }

// 후퇴 위치를 먼저 검증하고, 패배 직전이 아닌 조우 진입 시점의 체력·위치·자원을 복원한다.
bool UBattleCoordinator::RestoreEncounter(bool bRetry)
{
	if (!EncounterRecord.bReady || IsBattleOperationBusy() || CurrentState!=EBattleState::Defeat
		|| !ActivePlayerPawn.IsValid() || !ActiveEnemyActor.IsValid() || !ActiveEncounterSource.IsValid()) { return false; }
	APawn* Player=ExplorationPlayerPawn.Get();
	if (!Player) { return false; }
	const auto Enemies=GetBattleEnemies();
	TArray<APawn*> Allies;
	for (const auto& Member:TurnOrder) { if (Member.Get()!=Player && BattleAllies.Contains(Member)) { if (auto* Pawn=Cast<APawn>(Member.Get())) { Allies.Add(Pawn); } } }
	AActor* Source=ActiveEncounterSource.Get();
	auto* Trigger=Cast<ABattleEncounterTrigger>(Source);
	FVector RetreatLocation;
	if (!bRetry && (!Trigger || !Trigger->FindRetreatLocation(Player,RetreatLocation)))
	{
		UE_LOG(LogBattleCoordinator,Warning,TEXT("No safe retreat location. Set the encounter's fixed Retreat Point outside its volume."));
		return false;
	}
	const FBattleEncounterEntry Entry = EncounterRecord.Entry;
	if (!ReturnToExploration()) { return false; }
	Entry.RestoreParticipants();
	if (!bRetry)
	{
		Player->SetActorLocation(RetreatLocation,false,nullptr,ETeleportType::TeleportPhysics);
		Trigger->RearmAfterRetreat();
		return true;
	}
	if (!RequestGroupBattleStart(Source,Player,Enemies,Allies)) { return false; }
	Resources = Entry.Resources;
	EncounterRecord.Entry.Resources = Resources;
	for (const auto& Member : Entry.Participants)
	{
		if (Member.bAlly && Member.Actor.IsValid() && Member.Actor.Get()!=Player)
		{
			BattleAllies.AddUnique(Member.Actor); CaptureParticipant(Member.Actor.Get(),true);
		}
	}
	return true;
}
