// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BattleCoordinator.h"
#include "Combat/CharacterCombatData.h"
#include "Combat/CharacterActionExecutor.h"
#include "Combat/CombatantComponent.h"
#include "Combat/ParryCounterProvider.h"
#include "UI/BattlePresentationComponent.h"
#include "UI/DefensePresentationComponent.h"
#include "UI/EnemyAttackPresentationComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

void FBattleEnemyActionSession::Begin(int64 NewId)
{
	Id = NewId;
	HitIndex = 0;
	bAllHitsParried = true;
	bAttackStarted = false;
	ResetHit();
	// 구형 단타 패턴도 다단 공격과 같은 순번·방어 판정을 사용한다.
	if (Pattern.Hits.IsEmpty())
	{
		FEnemyAttackHit Hit;
		Hit.Damage = Pattern.Damage;
		Hit.HitTime = Pattern.HitTime;
		Hit.DefenseWindowDuration = Pattern.DefenseWindowDuration;
		Hit.ParrySuccessDuration = Pattern.ParrySuccessDuration;
		Hit.HitTimeOffset = Pattern.HitTimeOffset;
		Pattern.Hits.Add(Hit);
	}
}

void FBattleEnemyActionSession::PrepareCurrentHit()
{
	const FEnemyAttackHit& Hit = Pattern.Hits[HitIndex];
	Pattern.Damage = Hit.Damage;
	Pattern.HitTime = Hit.HitTime;
	Pattern.DefenseWindowDuration = Hit.DefenseWindowDuration;
	Pattern.ParrySuccessDuration = Hit.ParrySuccessDuration;
	Pattern.HitTimeOffset = Hit.HitTimeOffset;
	ResetHit();
}

void FBattleEnemyActionSession::ResetHit()
{
	BufferedDefense = EBattleDefenseResult::None;
	bHitResolved = false;
	bReceivedHitNotify = false;
}

void FBattleEnemyActionSession::ClearTimers(FTimerManager& Timers)
{
	Timers.ClearTimer(HitTimer);
	Timers.ClearTimer(WindowOpenTimer);
	Timers.ClearTimer(TimeoutTimer);
	Timers.ClearTimer(CounterTimer);
}

bool UBattleCoordinator::RequestParry()
{
	return RequestDefenseCommand(EBattleDefenseCommand::Parry);
}

bool UBattleCoordinator::RequestDodge()
{
	return RequestDefenseCommand(EBattleDefenseCommand::Dodge);
}

bool UBattleCoordinator::CanRequestDefenseCommand(const EBattleDefenseCommand Command) const
{
	if (CurrentState != EBattleState::DefenseWindow || !HasValidActiveBattle())
	{
		return false;
	}

	return !EnemyAction.bHitResolved && EnemyAction.BufferedDefense == EBattleDefenseResult::None
		&& GetEnemyHitTimeRemaining() > 0.0f
		&& (Command == EBattleDefenseCommand::Parry || Command == EBattleDefenseCommand::Dodge);
}

float UBattleCoordinator::GetEnemyHitTimeRemaining() const
{
	return GetWorld() && EnemyAction.Id && !EnemyAction.bHitResolved
		? FMath::Max(0.0f, GetWorld()->GetTimerManager().GetTimerRemaining(EnemyAction.HitTimer)) : 0.0f;
}

bool UBattleCoordinator::RequestDefenseCommand(const EBattleDefenseCommand Command)
{
	if (IsBattleOperationBusy())
	{
		LogRejectedCommand(TEXT("RequestDefenseCommand"), TEXT("another battle operation is in progress"));
		return false;
	}

	if (CurrentState != EBattleState::DefenseWindow)
	{
		LogRejectedCommand(TEXT("RequestDefenseCommand"), TEXT("current state is not DefenseWindow"));
		return false;
	}

	if (!HasValidActiveBattle())
	{
		LogRejectedCommand(TEXT("RequestDefenseCommand"), TEXT("active battle participants are invalid or defeated"));
		ReturnToExploration();
		return false;
	}

	if (Command != EBattleDefenseCommand::Parry && Command != EBattleDefenseCommand::Dodge)
	{
		LogRejectedCommand(TEXT("RequestDefenseCommand"), TEXT("the requested defense command is not supported"));
		return false;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		ReturnToExploration();
		return false;
	}

	const float Remaining = GetEnemyHitTimeRemaining();
	if (Remaining <= 0.0f || EnemyAction.bHitResolved || EnemyAction.BufferedDefense != EBattleDefenseResult::None)
	{
		LogRejectedCommand(TEXT("RequestDefenseCommand"), TEXT("hit already resolved or defense input already submitted"));
		return false;
	}

	EnemyAction.BufferedDefense = Command == EBattleDefenseCommand::Dodge
		? EBattleDefenseResult::DodgeSuccess
		: Remaining <= EnemyAction.Pattern.ParrySuccessDuration + KINDA_SMALL_NUMBER
			? EBattleDefenseResult::ParrySuccess
			: EBattleDefenseResult::Failed;
	UE_LOG(LogBattleCoordinator, Log, TEXT("Defense input buffered: Remaining=%.3f Result=%s"),
		Remaining, *UEnum::GetValueAsString(EnemyAction.BufferedDefense));
	TGuardValue<bool> ActionGuard(bActionInProgress, true);
	DefenseCommandAccepted.Broadcast(Command, Remaining);
	return true;
}

FEnemyAttackPatternSnapshot UBattleCoordinator::GetActiveEnemyPattern() const
{
	FEnemyAttackPatternSnapshot Result = EnemyAction.Pattern;
	if (!EnemyAction.bHasPattern)
	{
		Result.PatternId = NAME_None;
		Result.DisplayName = FText::GetEmpty();
	}
	return Result;
}

void UBattleCoordinator::ClearEnemyPattern()
{
	EnemyAction.bHasPattern = false;
	EnemyAction.Pattern = FEnemyAttackPatternSnapshot();
}

void UBattleCoordinator::SelectEnemyPattern()
{
	const auto LogInvalidPattern = [this](const UEnemyAttackPatternData* Pattern, const FString& Reason)
	{
		UE_LOG(LogBattleCoordinator, Warning, TEXT("Enemy '%s': skipping pattern '%s': %s"),
			*GetNameSafe(ActiveEnemyActor.Get()), *GetNameSafe(Pattern), *Reason);
	};
	// 이번 턴에 검증한 타격 시각과 피해량을 공격이 끝날 때까지 사용한다.
	const UEnemyAttackPatternData* Selected = UEnemyAttackPatternData::SelectPattern(
		EnemyCombatant->EnemyAttackPatterns, PatternRandom, &EnemyAction.Pattern, LogInvalidPattern);
	EnemyAction.bHasPattern = true;
	if (!Selected)
	{
		UE_LOG(LogBattleCoordinator, Warning,
			TEXT("Enemy '%s': no selectable enemy patterns (empty, invalid, or zero weights); using BasicAttack fallback."),
			*GetNameSafe(ActiveEnemyActor.Get()));
	}
	UE_LOG(LogBattleCoordinator, Log, TEXT("Enemy pattern selected: %s, Damage=%.3f Telegraph=%.3f Defense=%.3f Parry=%.3f"),
		*EnemyAction.Pattern.PatternId.ToString(), EnemyAction.Pattern.Damage, EnemyAction.Pattern.TelegraphDuration,
		EnemyAction.Pattern.DefenseWindowDuration, EnemyAction.Pattern.ParrySuccessDuration);
}

bool UBattleCoordinator::StartEnemyTelegraph()
{
	if (!HasValidActiveBattle())
	{
		ReturnToExploration();
		return false;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		ReturnToExploration();
		return false;
	}

	SetDefenseResult(EBattleDefenseResult::None);
	World->GetTimerManager().SetTimer(
		EnemyAction.TelegraphTimer,
		FTimerDelegate::CreateUObject(this, &ThisClass::HandleEnemyTelegraphFinished, BattleSerial),
		EnemyAction.Pattern.TelegraphDuration,
		false);
	return true;
}

void UBattleCoordinator::HandleEnemyTelegraphFinished(int64 ExpectedBattleId)
{
	if (ExpectedBattleId != BattleSerial || CurrentState != EBattleState::EnemyTelegraph) { return; }
	EnemyAction.TelegraphTimer.Invalidate();
	if (!HasValidActiveBattle()) { ReturnToExploration(); return; }
	if (!TryTransitionTo(EBattleState::EnemyAction)) { ReturnToExploration(); return; }
	EnemyAction.Begin(++NextEnemyActionId);
	EnemyAction.Presentation=ActiveEnemyActor->FindComponentByClass<UEnemyAttackPresentationComponent>();
	EnemyAction.bWaitingForAnimation=EnemyAction.Pattern.AttackMontage && EnemyAction.Presentation.IsValid();
	if (EnemyAction.bWaitingForAnimation)
	{
		EnemyAction.bWaitingForAnimation=EnemyAction.Presentation->BeginAttack(EnemyAction.Pattern.AttackMontage,EnemyAction.Id,ActivePlayerPawn.Get());
		if (!EnemyAction.bWaitingForAnimation && EnemyAction.Presentation->WasTravelBlocked())
		{
			NotifyEnemyActionFinished(EnemyAction.Id,true);return;
		}
	}
	if (!EnemyAction.bWaitingForAnimation) { NotifyEnemyAttackStarted(EnemyAction.Id); }
	if (EnemyAction.bWaitingForAnimation)
	{
		GetWorld()->GetTimerManager().SetTimer(EnemyAction.TimeoutTimer,
			FTimerDelegate::CreateUObject(this,&ThisClass::HandleEnemyActionTimeout,EnemyAction.Id),
			EnemyAction.Presentation->GetExpectedDuration(EnemyAction.Pattern.AttackMontage)+1.f,false);
	}
	UE_LOG(LogBattleCoordinator,Log,TEXT("Enemy combo started: Id=%lld Hits=%d"),EnemyAction.Id,EnemyAction.Pattern.Hits.Num());
}

void UBattleCoordinator::NotifyEnemyAttackStarted(const int64 ActionId)
{
	if (!EnemyAction.Matches(ActionId) || EnemyAction.bAttackStarted || CurrentState!=EBattleState::EnemyAction) { return; }
	EnemyAction.bAttackStarted=true;
	EnemyAction.AttackStartedAt=GetWorld()->GetTimeSeconds();
	ScheduleEnemyHit();
}

// 공격 시작 시각에 각 타격의 오프셋을 적용하고, 행동 ID와 타격 순번을 타이머에 묶는다.
void UBattleCoordinator::ScheduleEnemyHit()
{
	if (!EnemyAction.Pattern.Hits.IsValidIndex(EnemyAction.HitIndex) || !EnemyAction.Id) { return; }
	EnemyAction.PrepareCurrentHit();
	const FEnemyAttackHit& Hit = EnemyAction.Pattern.Hits[EnemyAction.HitIndex];
	auto& Timers=GetWorld()->GetTimerManager();
	Timers.ClearTimer(EnemyAction.HitTimer); Timers.ClearTimer(EnemyAction.WindowOpenTimer);
	const float Delay=FMath::Max(.001f,EnemyAction.AttackStartedAt+Hit.HitTime-GetWorld()->GetTimeSeconds());
	Timers.SetTimer(EnemyAction.HitTimer, FTimerDelegate::CreateUObject(this,
		&ThisClass::HandleDefenseWindowExpired, EnemyAction.Id, EnemyAction.HitIndex), Delay, false);
	if (Delay>Hit.DefenseWindowDuration+KINDA_SMALL_NUMBER)
	{
		Timers.SetTimer(EnemyAction.WindowOpenTimer, FTimerDelegate::CreateUObject(this,
			&ThisClass::OpenEnemyDefenseWindow, EnemyAction.Id, EnemyAction.HitIndex), Delay - Hit.DefenseWindowDuration, false);
	}
	else { OpenEnemyDefenseWindow(EnemyAction.Id, EnemyAction.HitIndex); }
}

void UBattleCoordinator::OpenEnemyDefenseWindow(int64 ActionId, int32 HitIndex)
{
	if (CurrentState != EBattleState::EnemyAction || !EnemyAction.MatchesHit(ActionId, HitIndex)) { return; }
	if (!HasValidActiveBattle()) { ReturnToExploration(); return; }
	TryTransitionTo(EBattleState::DefenseWindow);
}

void UBattleCoordinator::HandleDefenseWindowExpired(int64 ActionId, int32 HitIndex)
{
	if (!EnemyAction.MatchesHit(ActionId, HitIndex) || EnemyAction.bHitResolved) { return; }
	if (!HasValidActiveBattle())
	{
		ReturnToExploration();
		return;
	}
	if (CurrentState == EBattleState::EnemyAction) { OpenEnemyDefenseWindow(EnemyAction.Id, EnemyAction.HitIndex); }
	GetWorld()->GetTimerManager().ClearTimer(EnemyAction.HitTimer);
	TGuardValue<bool> ActionGuard(bActionInProgress, true);
	ResolveDefenseOutcome(EnemyAction.BufferedDefense == EBattleDefenseResult::None ? EBattleDefenseResult::NoInput : EnemyAction.BufferedDefense);
}

bool UBattleCoordinator::NotifyEnemyActionHit(const int64 ActionId, const int32 HitIndex)
{
	if (!EnemyAction.MatchesHit(ActionId, HitIndex) || EnemyAction.bHitResolved || EnemyAction.bReceivedHitNotify
		|| IsBattleOperationBusy()) { return false; }
	EnemyAction.bReceivedHitNotify = true;
	// 오프셋을 적용한 타격은 원본 몽타주의 마커가 먼저 도착해도 예약 시각에 판정한다.
	if (FMath::IsNearlyZero(EnemyAction.Pattern.HitTimeOffset) && GetEnemyHitTimeRemaining() <= 0.035f)
	{
		HandleDefenseWindowExpired(ActionId, EnemyAction.HitIndex);
	}
	return true;
}

bool UBattleCoordinator::NotifyEnemyActionFinished(const int64 ActionId, const bool bInterrupted)
{
	if (!EnemyAction.Matches(ActionId) || IsBattleOperationBusy()
		|| (CurrentState != EBattleState::EnemyAction && CurrentState != EBattleState::DefenseWindow)) { return false; }
	if (!bInterrupted && !EnemyAction.bHitResolved && EnemyAction.Pattern.Hits.Num()==1)
	{
		UE_LOG(LogBattleCoordinator, Warning, TEXT("Enemy hit marker/timer missing; resolving once at normal completion."));
		HandleDefenseWindowExpired(ActionId, EnemyAction.HitIndex);
		if (EnemyAction.Id != ActionId || CurrentState==EBattleState::Victory || CurrentState==EBattleState::Defeat) { return true; }
	}
	TGuardValue<bool> ActionGuard(bActionInProgress, true);
	if (!EnemyAction.bHitResolved) { EnemyAction.bAllHitsParried=false; }
	EnemyAction.bWaitingForAnimation = false;
	return FinishEnemyAction();
}

void UBattleCoordinator::HandleEnemyActionTimeout(const int64 ActionId)
{
	if (!EnemyAction.Matches(ActionId)) { return; }
	UE_LOG(LogBattleCoordinator, Warning, TEXT("Enemy animation completion timed out; cancelling remaining attack."));
	NotifyEnemyActionFinished(ActionId, true);
}

// 현재 방어자의 반격 실행기를 찾아 적의 복귀 연출을 잠시 멈춘다.
bool UBattleCoordinator::StartParryCounter()
{
 EnemyAction.CounterPresentation.Reset();
 if (const auto* Profile=GetCharacterActions())
 {
  EnemyAction.CounterAction=Profile->ParryCounter;
  if (!EnemyAction.CounterAction || !EnemyAction.CounterAction->IsValidAction()) { return false; }
  EnemyAction.CounterPresentation=FindActionExecutor(EnemyAction.CounterAction);
  if (!EnemyAction.CounterPresentation.IsValid()) { return false; }
  const float Duration=ICharacterActionExecutor::Execute_GetCharacterActionDuration(EnemyAction.CounterPresentation.Get(),EnemyAction.CounterAction,true);
  if (!FMath::IsFinite(Duration) || Duration<=0 || Duration>60) { EnemyAction.CounterPresentation.Reset(); return false; }
  EnemyAction.CounterDamage=EnemyAction.CounterAction->BaseDamage; EnemyAction.bCounterActive=true; EnemyAction.bCounterHitApplied=false;
  if (EnemyAction.Presentation.IsValid()) { EnemyAction.Presentation->HoldForCounter(); }
  if (BattlePresentation.IsValid() && BattlePresentation->BeginParryFeedback(EnemyAction.Id,true)) { return true; }
  return BeginCharacterCounter();
 }
 if (!IsContentFreeTest()) { return false; }
#if WITH_DEV_AUTOMATION_TESTS
 TInlineComponentArray<UActorComponent*> Components(ActivePlayerPawn.Get());
	for (UActorComponent* Component:Components)
	{
		if (!Component->GetClass()->ImplementsInterface(UParryCounterProvider::StaticClass())) { continue; }
		if (!PlayerCombatant->CounterProviderComponentName.IsNone() && Component->GetFName()!=PlayerCombatant->CounterProviderComponentName) { continue; }
		const float Duration=IParryCounterProvider::Execute_GetCounterActionDuration(Component);
		const float Damage=IParryCounterProvider::Execute_GetCounterActionDamage(Component);
		if (!FMath::IsFinite(Duration) || Duration<=0 || Duration>20 || !FMath::IsFinite(Damage) || Damage<0) { continue; }
		if (EnemyAction.Presentation.IsValid()) { EnemyAction.Presentation->HoldForCounter(); }
		EnemyAction.CounterPresentation=Component; EnemyAction.CounterDamage=Damage; EnemyAction.bCounterActive=true; EnemyAction.bCounterHitApplied=false;
		if (IParryCounterProvider::Execute_StartCounterAction(Component,EnemyAction.Id,ActiveEnemyActor.Get()))
		{
			GetWorld()->GetTimerManager().SetTimer(EnemyAction.CounterTimer,
				FTimerDelegate::CreateUObject(this,&ThisClass::HandleParryCounterTimeout,EnemyAction.Id),Duration+1.f,false);
			return true;
		}
		EnemyAction.bCounterActive=false;
		IParryCounterProvider::Execute_StopCounterAction(Component); EnemyAction.CounterPresentation.Reset();
	}
	// Content-free participants still work in logic tests. No particular spell is implied.
	const bool Applied=ApplyCombatEffect(EnemyCombatant.Get(),ParryCounterDamage,ECombatNumberKind::Damage);
	if (EnemyAction.Presentation.IsValid() && EnemyAction.Presentation->IsHeldForCounter()) { EnemyAction.bWaitingForAnimation=EnemyAction.Presentation->ReturnAfterCounter(); }
	return Applied;
#else
 return false;
#endif
}

// 한 타격의 방어를 한 번만 확정하고, 모든 타격을 패링했을 때 반격을 시작한다.
bool UBattleCoordinator::ResolveDefenseOutcome(const EBattleDefenseResult Result)
{
	if (CurrentState!=EBattleState::DefenseWindow || EnemyAction.bHitResolved) { return false; }
	EnemyAction.bHitResolved=true;
	EnemyAction.bAllHitsParried &= Result==EBattleDefenseResult::ParrySuccess;
	const bool bLastHit=EnemyAction.HitIndex+1>=EnemyAction.Pattern.Hits.Num();
	if (Result==EBattleDefenseResult::Failed || Result==EBattleDefenseResult::NoInput)
	{
		if (!ApplyCombatEffect(PlayerCombatant.Get(),EnemyAction.Pattern.Damage,ECombatNumberKind::Damage)) { ReturnToExploration(); return false; }
	}
	if (EnemyAction.Presentation.IsValid() && EnemyAction.Pattern.Hits.IsValidIndex(EnemyAction.HitIndex))
	{
		EnemyAction.Presentation->PlayHitFeedback(EnemyAction.Pattern.Hits[EnemyAction.HitIndex],Result);
	}
	if (bLastHit && EnemyAction.bAllHitsParried && !StartParryCounter()) { ReturnToExploration(); return false; }
	SetDefenseResult(Result);
	EncounterRecord.RecordDefense(Result);
	UE_LOG(LogBattleCoordinator,Log,TEXT("Enemy hit resolved: Id=%lld Hit=%d/%d Result=%s"),EnemyAction.Id,EnemyAction.HitIndex+1,EnemyAction.Pattern.Hits.Num(),*UEnum::GetValueAsString(Result));
	if (TryFinishBattle()) { return true; }
	if (!bLastHit)
	{
		if (PlayerCombatant->IsDefeated()) { EnemyAction.bWaitingForAnimation=false; return FinishEnemyAction(); }
		++EnemyAction.HitIndex;
		if (!TryTransitionTo(EBattleState::EnemyAction)) { ReturnToExploration(); return false; }
		ScheduleEnemyHit();
		return true;
	}
	return EnemyAction.bWaitingForAnimation || EnemyAction.bCounterActive ? true : FinishEnemyAction();
}

bool UBattleCoordinator::NotifyParryCounterHit(const int64 ActionId)
{
	if (!EnemyAction.bCounterActive || !EnemyAction.Matches(ActionId) || EnemyAction.bCounterHitApplied
		|| CurrentState != EBattleState::DefenseWindow || IsBattleOperationBusy()) { return false; }
	if (!HasValidActiveBattle()) { ReturnToExploration(); return false; }
	TGuardValue<bool> Guard(bActionInProgress, true);
	EnemyAction.bCounterHitApplied = true;
	// Stop the enemy's recovery before the counter's additive hit reaction is played.
	if (EnemyAction.Presentation.IsValid() && !EnemyAction.Presentation->IsHeldForCounter()) { EnemyAction.Presentation->StopPresentation(); EnemyAction.bWaitingForAnimation=false; }
	if (!ApplyCombatEffect(EnemyCombatant.Get(), EnemyAction.CounterDamage, ECombatNumberKind::Damage)) { ReturnToExploration(); return false; }
	// Death cancels the attack montage; it will no longer send an attack-finished callback.
	if (EnemyCombatant->IsDefeated()) { EnemyAction.bWaitingForAnimation=false; }
	UE_LOG(LogBattleCoordinator, Log, TEXT("Parry counter impact: Id=%lld Damage=%.1f"), ActionId, EnemyAction.CounterDamage);
	TryFinishBattle();
	return true;
}

bool UBattleCoordinator::NotifyParryCounterFinished(const int64 ActionId, const bool bInterrupted)
{
	if (!EnemyAction.bCounterActive || !EnemyAction.Matches(ActionId)
		|| CurrentState != EBattleState::DefenseWindow || IsBattleOperationBusy()) { return false; }
	TGuardValue<bool> Guard(bActionInProgress, true);
	EnemyAction.bCounterActive = false;
	GetWorld()->GetTimerManager().ClearTimer(EnemyAction.CounterTimer);
	StopCounterPresentation();
	EnemyAction.CounterPresentation.Reset();
	UE_LOG(LogBattleCoordinator, Log, TEXT("Parry counter finished: Id=%lld Interrupted=%d"), ActionId, bInterrupted);
	if (EnemyAction.Presentation.IsValid() && EnemyAction.Presentation->IsHeldForCounter()) { EnemyAction.bWaitingForAnimation=EnemyAction.Presentation->ReturnAfterCounter(); }
	return EnemyAction.bWaitingForAnimation ? true : FinishEnemyAction();
}

void UBattleCoordinator::HandleParryCounterTimeout(const int64 ActionId)
{
	if (EnemyAction.bCounterActive && ActionId == EnemyAction.Id)
	{
		UE_LOG(LogBattleCoordinator, Warning, TEXT("Parry counter timed out; cancelling remaining counter."));
		NotifyParryCounterFinished(ActionId, true);
	}
}

bool UBattleCoordinator::FinishEnemyAction()
{
	if (CurrentState==EBattleState::Victory || CurrentState==EBattleState::Defeat) { return true; }
	if (EnemyAction.bCounterActive) { return true; }
	if (!ActivePlayerPawn.IsValid() || !ActiveEnemyActor.IsValid() || !PlayerCombatant.IsValid() || !EnemyCombatant.IsValid())
	{
		ReturnToExploration();
		return false;
	}
	ClearEnemyAction();
	ClearEnemyPattern();
	if (!TryTransitionTo(EBattleState::TurnEnd))
	{
		ReturnToExploration();
		return false;
	}

	return ResolveTurnEnd(ECompletedAction::Enemy);
}

// 반격을 먼저 해제하고 예약된 타격과 몽타주 통지가 다음 턴으로 넘어가지 않게 정리한다.
void UBattleCoordinator::ClearEnemyAction()
{
	if (BattlePresentation.IsValid()) { BattlePresentation->ReleaseParryFeedback(); }
	EnemyAction.bCounterActive = false;
	EnemyAction.bCounterHitApplied = false;
	StopCounterPresentation();
	EnemyAction.CounterPresentation.Reset();
	EnemyAction.Id = 0;
	EnemyAction.HitIndex=0; EnemyAction.bAllHitsParried=true; EnemyAction.bAttackStarted=false;
	if (GetWorld())
	{
		EnemyAction.ClearTimers(GetWorld()->GetTimerManager());
	}
	if (EnemyAction.Presentation.IsValid()) { EnemyAction.Presentation->StopPresentation(); }
	EnemyAction.Presentation.Reset();
	EnemyAction.ResetHit();
	EnemyAction.bWaitingForAnimation = false;
}

void UBattleCoordinator::SetDefenseResult(const EBattleDefenseResult Result)
{
	if (LastDefenseResult == Result && Result==EBattleDefenseResult::None)
	{
		return;
	}

	LastDefenseResult = Result;
	DefenseResultChanged.Broadcast(Result);
	DefenseResultChangedBlueprint.Broadcast(Result);
}

void UBattleCoordinator::StopCounterPresentation()
{
 if (!EnemyAction.CounterPresentation.IsValid()) { return; }
 if (EnemyAction.CounterAction && EnemyAction.CounterPresentation->GetClass()->ImplementsInterface(UCharacterActionExecutor::StaticClass()))
 { ICharacterActionExecutor::Execute_StopCharacterAction(EnemyAction.CounterPresentation.Get()); }
 else if (EnemyAction.CounterPresentation->GetClass()->ImplementsInterface(UParryCounterProvider::StaticClass()))
 { IParryCounterProvider::Execute_StopCounterAction(EnemyAction.CounterPresentation.Get()); }
 EnemyAction.CounterAction=nullptr;
}

bool UBattleCoordinator::BeginCharacterCounter()
{
 if (!EnemyAction.bCounterActive || !EnemyAction.CounterAction || !EnemyAction.CounterPresentation.IsValid()) { return false; }
	if (BattlePresentation.IsValid()) { BattlePresentation->PresentCounterAction(EnemyAction.CounterAction); }
 if (auto* Defense=ActivePlayerPawn->FindComponentByClass<UDefensePresentationComponent>()) { Defense->StopPresentation(); }
 const float Duration=ICharacterActionExecutor::Execute_GetCharacterActionDuration(EnemyAction.CounterPresentation.Get(),EnemyAction.CounterAction,true);
 if (ICharacterActionExecutor::Execute_StartCharacterAction(EnemyAction.CounterPresentation.Get(),EnemyAction.CounterAction,EnemyAction.Id,ActiveEnemyActor.Get(),true))
 {
  GetWorld()->GetTimerManager().SetTimer(EnemyAction.CounterTimer,FTimerDelegate::CreateUObject(this,&ThisClass::HandleParryCounterTimeout,EnemyAction.Id),Duration+1,false);
  return true;
 }
 EnemyAction.bCounterActive=false; StopCounterPresentation(); EnemyAction.CounterPresentation.Reset(); return false;
}

void UBattleCoordinator::NotifyCounterFeedbackFinished(int64 ActionId)
{
 if (!EnemyAction.bCounterActive || !EnemyAction.Matches(ActionId) || CurrentState!=EBattleState::DefenseWindow) { return; }
 if (!HasValidActiveBattle() || !BeginCharacterCounter()) { ReturnToExploration(); }
}
