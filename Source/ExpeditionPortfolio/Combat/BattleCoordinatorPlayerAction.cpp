// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BattleCoordinator.h"
#include "Combat/ActionQTESubsystem.h"
#include "Combat/BattleRulesData.h"
#include "Combat/CharacterActionExecutor.h"
#include "Combat/CharacterCombatData.h"
#include "Combat/CombatantComponent.h"
#include "Combat/CombatItemData.h"
#include "UI/ItemPresentationComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "UI/MagicActionData.h"
#include "UI/MagicPresentationComponent.h"
#include "UI/PlayerUtilityPresentationComponent.h"
#endif

void UBattleCoordinator::SetPlayerActionPhase(EPlayerActionPhase Phase)
{
	if (PlayerAction.Phase == Phase) { return; }
	PlayerAction.Phase = Phase;
	PlayerPhaseChanged.Broadcast(Phase);
}

void UBattleCoordinator::BeginPlayerPreparation(int64 ActionId, float Duration, float SuccessDuration, float Multiplier, bool bTimingEnabled, const UActionQTEPatternData* Pattern)
{
	if (!PlayerAction.Matches(ActionId) || CurrentState != EBattleState::PlayerAction) { return; }
	SetPlayerActionPhase(EPlayerActionPhase::Preparing);
	if (bTimingEnabled && !PlayerAction.QTESessionId)
	{
		auto* QTE = GetWorld()->GetSubsystem<UActionQTESubsystem>();
		PlayerAction.QTESessionId = Pattern ? QTE->BeginPattern(Pattern) : QTE->BeginQTE(Duration, SuccessDuration, Multiplier);
		if (!PlayerAction.QTESessionId) { UE_LOG(LogBattleCoordinator, Warning, TEXT("Invalid or busy QTE; preparing without bonus.")); }
		AttackTimingChanged.Broadcast();
	}
}

void UBattleCoordinator::NotifyPlayerPreparationFinished(int64 ActionId)
{
	if (!PlayerAction.Matches(ActionId) || CurrentState != EBattleState::PlayerAction || PlayerAction.Phase != EPlayerActionPhase::Preparing) { return; }
	GetWorld()->GetSubsystem<UActionQTESubsystem>()->FinishWindow(PlayerAction.QTESessionId);
	SetPlayerActionPhase(EPlayerActionPhase::Executing);
}

bool UBattleCoordinator::RequestAttackTimingInput()
{
	if (IsBattleOperationBusy() || CurrentState != EBattleState::PlayerAction || !PlayerAction.Id
		|| PlayerAction.Phase != EPlayerActionPhase::Preparing || !PlayerAction.QTESessionId) { return false; }
	RefreshAreaPrimaryTarget();
	if (!HasValidActiveBattle()) { ReturnToExploration(); return false; }
	TGuardValue<bool> Guard(bActionInProgress, true);
	return GetWorld()->GetSubsystem<UActionQTESubsystem>()->SubmitInput(PlayerAction.QTESessionId);
}

FAttackTimingView UBattleCoordinator::GetAttackTimingView() const
{
	const auto View = GetWorld()->GetSubsystem<UActionQTESubsystem>()->GetView();
	return PlayerAction.QTESessionId && PlayerAction.QTESessionId == View.SessionId ? View : FAttackTimingView();
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
	if (bConfirmingTarget || CurrentState != EBattleState::PlayerCommand
		|| !HasValidActiveBattle())
	{
		return false;
	}

 if (IsItemCommand(Command)) { return CanUseItem(Command==EBattlePlayerCommand::UsePotion?GetDefaultItem():PendingItem?PendingItem.Get():GetDefaultItem()); }
 if (GetCharacterActions()) { return CanRequestAction(ResolveCommandAction(Command)); }
 if (!IsContentFreeTest()) { return false; }
 switch (Command)
 {
 case EBattlePlayerCommand::BasicAttack:
	case EBattlePlayerCommand::MeleeAttack:
		return true;
	case EBattlePlayerCommand::UsePotion:
		if (RemainingPotions<=0) { return false; }
		if (CanHealAlly(ActivePlayerPawn.Get())) { return true; }
		for (const auto& Ally:BattleAllies) { if (CanHealAlly(Ally.Get())) { return true; } }
		return false;

	case EBattlePlayerCommand::PowerStrike:
		return !bPowerStrikeUsed;

	default:
		return false;
	}
}

bool UBattleCoordinator::RequestPlayerCommand(const EBattlePlayerCommand Command)
{
	if (bSelectingTarget || bConfirmingTarget || bTargetNotification) { return false; }
	if (IsBattleOperationBusy())
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

	if (!GetCharacterActions() && Command == EBattlePlayerCommand::PowerStrike && bPowerStrikeUsed)
	{
		LogRejectedCommand(TEXT("RequestPlayerCommand"), TEXT("PowerStrike has already been used in this battle"));
		return false;
	}

	if (!CanRequestPlayerCommand(Command))
	{
		LogRejectedCommand(TEXT("RequestPlayerCommand"), TEXT("command unavailable (unsupported, full health, or no items)"));
		return false;
	}

	return ExecutePlayerCommand(Command);
}

float UBattleCoordinator::GetMagicDamage(const EBattlePlayerCommand Command) const
{
	if (Command != EBattlePlayerCommand::BasicAttack && Command != EBattlePlayerCommand::PowerStrike) { return 0.0f; }
	if (auto* Data=ResolveCommandAction(Command)) { return Data->BaseDamage; }
	if (!IsContentFreeTest()) { return 0; }
#if WITH_DEV_AUTOMATION_TESTS
	const float Fallback = Command == EBattlePlayerCommand::PowerStrike ? PlayerPowerStrikeDamage : PlayerBasicAttackDamage;
	const UMagicPresentationComponent* Magic = ActivePlayerPawn.IsValid()
		? ActivePlayerPawn->FindComponentByClass<UMagicPresentationComponent>() : nullptr;
	const UMagicActionData* Data = Magic
		? (Command == EBattlePlayerCommand::PowerStrike ? Magic->StrongMagic.Get() : Magic->BasicMagic.Get()) : nullptr;
	return Data && FMath::IsFinite(Data->BaseDamage) && Data->BaseDamage >= 0.0f ? Data->BaseDamage : Fallback;
#else
	return 0;
#endif
}

// 대상을 확정한 명령이 실행 단계에 진입하면 사용 횟수와 아이템을 소모한다.
bool UBattleCoordinator::ExecutePlayerCommand(const EBattlePlayerCommand Command)
{
	UCharacterActionData* Action = IsItemCommand(Command) ? nullptr : ResolveCommandAction(Command);
	UCombatItemData* Item = nullptr;
	if (IsItemCommand(Command))
	{
		Item = Command == EBattlePlayerCommand::UsePotion || !PendingItem ? GetDefaultItem() : PendingItem.Get();
	}
	const auto Scope = Action ? Action->TargetScope : ECharacterActionTargetScope::SingleEnemy;
	TArray<TWeakObjectPtr<AActor>> TargetsForAction;
	if (!Item)
	{
		if (Scope == ECharacterActionTargetScope::AllEnemies)
		{
			for (const auto& Enemy : BattleEnemies)
			{
				if (IsRegisteredEnemy(Enemy.Get())) { TargetsForAction.Add(Enemy); }
			}
		}
		else if (IsRegisteredEnemy(ActiveEnemyActor.Get()))
		{
			TargetsForAction.Add(ActiveEnemyActor);
		}
		if (TargetsForAction.IsEmpty()) { return false; }
	}

	const float Amount = Action ? Action->BaseDamage : Item ? Item->HealingAmount :
		Command == EBattlePlayerCommand::MeleeAttack ? MeleeDamage : GetMagicDamage(Command);
	TGuardValue<bool> ActionGuard(bActionInProgress, true);
	if (!TryTransitionTo(EBattleState::PlayerAction)) { return false; }
	PlayerAction.Action = Action;
	PlayerAction.TargetScope = Scope;
	PlayerAction.Targets = MoveTemp(TargetsForAction);
	if (Action) { ++ActionUses.FindOrAdd(ActivePlayerPawn.Get()).FindOrAdd(Action->ActionId); }
	else if (Command == EBattlePlayerCommand::PowerStrike) { bPowerStrikeUsed = true; }

	PlayerAction.Id = ++NextPlayerActionId;
	PlayerAction.QTESessionId = 0;
	AttackTimingChanged.Broadcast();
	FBattleFacingRestore::Face(ActivePlayerPawn.Get(), ActiveEnemyActor.Get());
	FBattleFacingRestore::Face(ActiveEnemyActor.Get(), ActivePlayerPawn.Get());
	PlayerAction.Command = Command;
	PlayerAction.Item = Item;
	if (Item)
	{
		--ItemStocks.FindChecked(Item->ItemId);
		RemainingPotions = GetRemainingItems(GetDefaultItem());
	}
	PlayerAction.EffectAmount = Amount;
	PlayerAction.bHitApplied = false;
	if (TryStartPlayerPresentation()) { return true; }

	// 연출이 시작되지 않았으면 외부 콜백과 같은 종료 경로로 한 번만 정리한다.
	bActionInProgress = false;
	return NotifyPlayerActionFinished(PlayerAction.Id, !IsContentFreeTest());
}

bool UBattleCoordinator::TryStartPlayerPresentation()
{
	float Expected = 0.0f;
	auto HasValidDuration = [](float Duration)
	{
		return FMath::IsFinite(Duration) && Duration > 0.0f && Duration < 60.0f;
	};
	if (auto* Action = PlayerAction.Action.Get())
	{
		auto* Executor = FindActionExecutor(Action);
		PlayerAction.ActionPresentation = Executor;
		if (Executor)
		{
			Expected = ICharacterActionExecutor::Execute_GetCharacterActionDuration(Executor, Action, false);
		}
		if (!Executor || !HasValidDuration(Expected) || !ICharacterActionExecutor::Execute_StartCharacterAction(
			Executor, Action, PlayerAction.Id, ActiveEnemyActor.Get(), false))
		{
			UE_LOG(LogBattleCoordinator, Warning, TEXT("Character action presentation unavailable: %s"), *GetNameSafe(Action));
			return false;
		}
		if (PlayerAction.Phase != EPlayerActionPhase::Preparing) { SetPlayerActionPhase(EPlayerActionPhase::Executing); }
	}
	else if (auto* Item = PlayerAction.Item.Get())
	{
		SetPlayerActionPhase(EPlayerActionPhase::Executing);
		auto* Presentation = ActivePlayerPawn->FindComponentByClass<UItemPresentationComponent>();
		PlayerAction.ItemPresentation = Presentation;
		if (!Presentation) { return false; }
		Expected = Presentation->GetExpectedDuration();
		if (!HasValidDuration(Expected) || !Presentation->BeginUse(Item, PlayerAction.Id,
			ItemTarget.IsValid() ? ItemTarget.Get() : ActivePlayerPawn.Get())) { return false; }
	}
#if WITH_DEV_AUTOMATION_TESTS
	else if (IsContentFreeTest() && (PlayerAction.Command == EBattlePlayerCommand::MeleeAttack || IsItemCommand(PlayerAction.Command)))
	{
		SetPlayerActionPhase(EPlayerActionPhase::Executing);
		auto* Presentation = ActivePlayerPawn->FindComponentByClass<UPlayerUtilityPresentationComponent>();
		PlayerAction.UtilityPresentation = Presentation;
		if (!Presentation) { return false; }
		Expected = Presentation->GetExpectedDuration(PlayerAction.Command);
		AActor* Target = IsItemCommand(PlayerAction.Command) ? (ItemTarget.IsValid() ? ItemTarget.Get() : ActivePlayerPawn.Get()) : ActiveEnemyActor.Get();
		if (!HasValidDuration(Expected) || !Presentation->BeginAction(PlayerAction.Command, PlayerAction.Id, Target))
		{
			UE_LOG(LogBattleCoordinator, Warning, TEXT("Utility presentation unavailable; resolving accepted command without animation."));
			return false;
		}
	}
	else if (IsContentFreeTest())
	{
		auto* Presentation = ActivePlayerPawn->FindComponentByClass<UMagicPresentationComponent>();
		PlayerAction.MagicPresentation = Presentation;
		if (!Presentation) { return false; }
		Expected = Presentation->GetExpectedDuration(PlayerAction.Command);
		if (!HasValidDuration(Expected) || !Presentation->BeginMagicAction(PlayerAction.Command, PlayerAction.Id, ActiveEnemyActor.Get()))
		{
			UE_LOG(LogBattleCoordinator, Warning, TEXT("Magic presentation failed; resolving accepted command without animation."));
			return false;
		}
		if (PlayerAction.Phase != EPlayerActionPhase::Preparing) { SetPlayerActionPhase(EPlayerActionPhase::Executing); }
	}
#endif
	else { return false; }

	GetWorld()->GetTimerManager().SetTimer(PlayerAction.TimeoutTimer,
		FTimerDelegate::CreateUObject(this, &ThisClass::HandlePlayerActionTimeout, PlayerAction.Id), Expected + 1.0f, false);
	return true;
}

bool UBattleCoordinator::NotifyPlayerActionHit(const int64 ActionId)
{
	if (!PlayerAction.Matches(ActionId) || CurrentState != EBattleState::PlayerAction
		|| IsBattleOperationBusy() || !PlayerAction.CanApplyHit())
	{
		return false;
	}
	TGuardValue<bool> ActionGuard(bActionInProgress, true);
	RefreshAreaPrimaryTarget();
	if (!HasValidActiveBattle())
	{
		ReturnToExploration();
		return false;
	}
	PlayerAction.bHitApplied = true;
	const float Multiplier = GetWorld()->GetSubsystem<UActionQTESubsystem>()->GetDamageMultiplier(PlayerAction.QTESessionId);
	const float AppliedAmount = PlayerAction.EffectAmount * Multiplier;
	AActor* HealTarget=ItemTarget.IsExplicitlyNull()?ActivePlayerPawn.Get():ItemTarget.Get();
	if (IsItemCommand(PlayerAction.Command) && !CanHealAlly(HealTarget)) { return false; }
	bool bApplied=false;
	if (IsItemCommand(PlayerAction.Command))
	{
		bApplied=ApplyCombatEffect(HealTarget->FindComponentByClass<UCombatantComponent>(),AppliedAmount,ECombatNumberKind::Healing);
	}
	else
	{
		// 첫 적이 쓰러져도 확정된 광역 대상 모두에게 피해를 적용한 뒤 승패를 검사한다.
		const auto HitTargets=GetActiveActionTargets();
		for (AActor* Target:HitTargets)
		{
			if (!IsRegisteredEnemy(Target)) { continue; }
			bApplied=ApplyCombatEffect(Target->FindComponentByClass<UCombatantComponent>(),AppliedAmount,
				Multiplier>1.f?ECombatNumberKind::BonusDamage:ECombatNumberKind::Damage) || bApplied;
		}
	}
	if (!bApplied)
	{
		ReturnToExploration();
		return false;
	}
	UE_LOG(LogBattleCoordinator, Log, TEXT("Player effect applied: Action=%lld Command=%s Amount=%.1f"),
		ActionId, *UEnum::GetValueAsString(PlayerAction.Command), AppliedAmount);
	TryFinishBattle();
	return true;
}

bool UBattleCoordinator::NotifyPlayerActionFinished(const int64 ActionId, const bool bInterrupted)
{
	if (!PlayerAction.Matches(ActionId) || CurrentState != EBattleState::PlayerAction
		|| IsBattleOperationBusy())
	{
		return false;
	}
	// 정상 종료에서 빠진 타격만 보충한다. 중단된 행동의 사용 횟수는 그대로 소모된다.
	if (!bInterrupted && !PlayerAction.bHitApplied)
	{

#if WITH_DEV_AUTOMATION_TESTS
		if (PlayerAction.MagicPresentation.IsValid())
		{
			UE_LOG(LogBattleCoordinator, Warning, TEXT("Magic hit notify missing; applying one hit at normal completion."));
		}

#endif
		if (!NotifyPlayerActionHit(ActionId)) { return false; }
	}
	if (CurrentState == EBattleState::Victory || CurrentState == EBattleState::Defeat) { return true; }
	TGuardValue<bool> ActionGuard(bActionInProgress, true);
	if (!ActivePlayerPawn.IsValid() || !ActiveEnemyActor.IsValid() || !PlayerCombatant.IsValid() || !EnemyCombatant.IsValid())
	{
		ReturnToExploration();
		return false;
	}
	ClearPlayerAction();
	if (!TryTransitionTo(EBattleState::TurnEnd)) { ReturnToExploration(); return false; }
	return ResolveTurnEnd(ECompletedAction::Player);
}

void UBattleCoordinator::HandlePlayerActionTimeout(const int64 ActionId)
{
	if (ActionId != PlayerAction.Id || CurrentState != EBattleState::PlayerAction) { return; }
	UE_LOG(LogBattleCoordinator, Warning, TEXT("Magic action completion timed out; cancelling presentation."));
	NotifyPlayerActionFinished(ActionId, true);
}

void UBattleCoordinator::ClearPlayerAction()
{
	PlayerAction.Targets.Reset();
	PlayerAction.TargetScope = ECharacterActionTargetScope::SingleEnemy;
	ItemTarget.Reset();
	if (GetWorld()) { GetWorld()->GetSubsystem<UActionQTESubsystem>()->CancelQTE(PlayerAction.QTESessionId); }
	PlayerAction.QTESessionId = 0;
	SetPlayerActionPhase(EPlayerActionPhase::None);
	PlayerAction.Stop(GetWorld());
}

void FBattlePlayerActionSession::Stop(UWorld* World)
{
	// 정지 중 발생하는 몽타주 콜백이 이전 행동을 완료하지 못하도록 ID를 먼저 무효화한다.
	Id = 0;
	if (World) { World->GetTimerManager().ClearTimer(TimeoutTimer); }
	if (ItemPresentation.IsValid()) { ItemPresentation->StopPresentation(); }
	ItemPresentation.Reset();
	Item = nullptr;
	if (ActionPresentation.IsValid()) { ICharacterActionExecutor::Execute_StopCharacterAction(ActionPresentation.Get()); }
	ActionPresentation.Reset();
	Action = nullptr;
#if WITH_DEV_AUTOMATION_TESTS
	if (MagicPresentation.IsValid()) { MagicPresentation->StopPresentation(); }
	if (UtilityPresentation.IsValid()) { UtilityPresentation->StopPresentation(); }
	UtilityPresentation.Reset();
	MagicPresentation.Reset();
#endif
	EffectAmount = 0.0f;
	bHitApplied = false;
}

UCharacterCombatData* UBattleCoordinator::GetCharacterActions() const
{ return PlayerCombatant.IsValid()?PlayerCombatant->CharacterActions.Get():nullptr; }

UCharacterActionData* UBattleCoordinator::ResolveCommandAction(EBattlePlayerCommand Command) const
{
 const auto* Profile=GetCharacterActions();
 if (!Profile) { return nullptr; }
 if (Command==EBattlePlayerCommand::CharacterAction) { return PendingAction; }
 if (Command==EBattlePlayerCommand::MeleeAttack) { return Profile->BasicAttack; }
 const int32 Index=Command==EBattlePlayerCommand::BasicAttack?0:Command==EBattlePlayerCommand::PowerStrike?1:INDEX_NONE;
 return Profile->Skills.IsValidIndex(Index)?Profile->Skills[Index].Get():nullptr;
}

float UBattleCoordinator::GetMeleeDamage() const
{ const auto* Action=ResolveCommandAction(EBattlePlayerCommand::MeleeAttack); return Action?Action->BaseDamage:IsContentFreeTest()?MeleeDamage:0; }

int32 UBattleCoordinator::GetRemainingActionUses(UCharacterActionData* Action) const
{
 if (!Action) { return 0; }
 if (Action->UsesPerEncounter==0) { return -1; }
 const auto* Uses=ActionUses.Find(ActivePlayerPawn.Get());
 return FMath::Max(0,Action->UsesPerEncounter-(Uses?Uses->FindRef(Action->ActionId):0));
}

bool UBattleCoordinator::IsActionExhausted(UCharacterActionData* Action) const
{ return Action && Action->UsesPerEncounter>0 && GetRemainingActionUses(Action)==0; }

bool UBattleCoordinator::IsCommandExhausted(EBattlePlayerCommand Command) const
{ return GetCharacterActions()?IsActionExhausted(ResolveCommandAction(Command)):IsContentFreeTest() && Command==EBattlePlayerCommand::PowerStrike && bPowerStrikeUsed; }

UActorComponent* UBattleCoordinator::FindActionExecutor(UCharacterActionData* Action) const
{
 if (!Action || !ActivePlayerPawn.IsValid()) { return nullptr; }
 TInlineComponentArray<UActorComponent*> Components(ActivePlayerPawn.Get());
 for (UActorComponent* Component:Components)
 {
  if ((!Action->ExecutorComponentName.IsNone() && Component->GetFName()!=Action->ExecutorComponentName)
   || !Component->GetClass()->ImplementsInterface(UCharacterActionExecutor::StaticClass())) { continue; }
  if (ICharacterActionExecutor::Execute_SupportsCharacterAction(Component,Action)) { return Component; }
 }
 return nullptr;
}

bool UBattleCoordinator::CanRequestAction(UCharacterActionData* Action) const
{
 const auto* Profile=GetCharacterActions();
 return CurrentState==EBattleState::PlayerCommand && !bConfirmingTarget && HasValidActiveBattle()
  && Profile && Profile->IsValidConfiguration() && Profile->ContainsCommand(Action) && !IsActionExhausted(Action)
  && (IsContentFreeTest() || (FindActionExecutor(Action) && ICharacterActionExecutor::Execute_GetCharacterActionDuration(FindActionExecutor(Action),Action,false)>0));
}

bool UBattleCoordinator::RequestPlayerAction(UCharacterActionData* Action)
{
 if (bSelectingTarget || bConfirmingTarget || bTargetNotification || IsBattleOperationBusy() || !CanRequestAction(Action)) { return false; }
 PendingAction=Action;
 return ExecutePlayerCommand(EBattlePlayerCommand::CharacterAction);
}

bool UBattleCoordinator::BeginActionSelection(UCharacterActionData* Action)
{
 if (bSelectingTarget || bConfirmingTarget || bTargetNotification || IsBattleOperationBusy() || !CanRequestAction(Action)) { return false; }
 PendingAction=Action;
 if (BeginCommandSelection(EBattlePlayerCommand::CharacterAction)) { return true; }
 PendingAction=nullptr; return false;
}

TArray<UCombatItemData*> UBattleCoordinator::GetAvailableItems() const
{
 TArray<UCombatItemData*> Result;
 if (ActiveRules) { for (const auto& Stock:ActiveRules->InitialItems) { if (Stock.Item) { Result.Add(Stock.Item); } } }
 return Result;
}

UCombatItemData* UBattleCoordinator::GetDefaultItem() const
{ return ActiveRules && !ActiveRules->InitialItems.IsEmpty()?ActiveRules->InitialItems[0].Item.Get():nullptr; }

int32 UBattleCoordinator::GetRemainingItems(UCombatItemData* Item) const
{ return Item?ItemStocks.FindRef(Item->ItemId):0; }

bool UBattleCoordinator::CanUseItem(UCombatItemData* Item) const
{
 if (!Item || !Item->IsValidConfiguration() || CurrentState!=EBattleState::PlayerCommand || bConfirmingTarget || !HasValidActiveBattle()
  || !GetAvailableItems().Contains(Item) || GetRemainingItems(Item)<=0) { return false; }
 if (!IsContentFreeTest())
 {
  const auto* Presentation=ActivePlayerPawn->FindComponentByClass<UItemPresentationComponent>();
  if (!Presentation || Presentation->GetExpectedDuration()<=0) { return false; }
 }
 if (CanHealAlly(ActivePlayerPawn.Get())) { return true; }
 for (const auto& Ally:BattleAllies) { if (CanHealAlly(Ally.Get())) { return true; } }
 return false;
}

bool UBattleCoordinator::BeginItemSelection(UCombatItemData* Item)
{
 if (bSelectingTarget || bConfirmingTarget || bTargetNotification || IsBattleOperationBusy() || !CanUseItem(Item)) { return false; }
 PendingItem=Item;
 if (BeginCommandSelection(EBattlePlayerCommand::UseItem)) { return true; }
 PendingItem=nullptr;return false;
}
