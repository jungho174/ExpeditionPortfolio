// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BattleCoordinator.h"
#include "Combat/CharacterCombatData.h"
#include "Combat/BattleRulesData.h"
#include "Combat/CombatItemData.h"
#include "ExpeditionPortfolioGameMode.h"
#include "Combat/CharacterActionExecutor.h"

#include "Combat/CombatantComponent.h"
#include "Combat/ActionQTESubsystem.h"
#include "Combat/PlayerCommandData.h"
#include "UI/BattlePresentationComponent.h"
#include "UI/BattleResultPresentationComponent.h"
#include "Exploration/Encounters/BattleEncounterTrigger.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "UObject/EnumProperty.h"

DEFINE_LOG_CATEGORY(LogBattleCoordinator);

void UBattleCoordinator::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UActionQTESubsystem>();
	QTEChangedHandle = GetWorld()->GetSubsystem<UActionQTESubsystem>()->Changed.AddLambda([this] { AttackTimingChanged.Broadcast(); });
	CurrentState = EBattleState::Exploration;
	PatternRandom.GenerateNewSeed();
	ClearBattleContext();
	bTransitionInProgress = false;
	bActionInProgress = false;
}

void UBattleCoordinator::Deinitialize()
{
	CombatNumberApplied.Clear();
	CombatNumberAppliedBlueprint.Clear();
	TargetSelectionChanged.Clear();
	GetWorld()->GetSubsystem<UActionQTESubsystem>()->Changed.Remove(QTEChangedHandle);
	PlayerPhaseChanged.Clear();
	AttackTimingChanged.Clear();
	BattleStateChanged.Clear();
	BattleStateChangedBlueprint.Clear();
	DefenseResultChanged.Clear();
	DefenseCommandAccepted.Clear();
	DefenseResultChangedBlueprint.Clear();
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
	return RequestGroupBattleStart(EncounterSource, PlayerPawn, {EnemyActor}, {});
}

// 모든 참가자와 전투 규칙을 검증한 뒤에만 진입 상태와 턴 순서를 등록한다.
bool UBattleCoordinator::RequestGroupBattleStart(AActor* EncounterSource, APawn* PlayerPawn, const TArray<AActor*>& Enemies, const TArray<APawn*>& AdditionalAllies)
{
	AActor* EnemyActor = Enemies.IsEmpty() ? nullptr : Enemies[0];
	if (IsBattleOperationBusy())
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
	if (const auto* Trigger=Cast<ABattleEncounterTrigger>(EncounterSource); Trigger && Trigger->IsEncounterCleared())
	{
		UE_LOG(LogBattleCoordinator,Verbose,TEXT("Rejected battle start request: encounter is already cleared."));
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
	if ((AdditionalAllies.IsEmpty() && FoundPlayerCombatant->IsDefeated()) || FoundEnemyCombatant->IsDefeated())
	{
		UE_LOG(
			LogBattleCoordinator,
			Warning,
			TEXT("Rejected battle start request: Player and Enemy must both be able to act."));
		return false;
	}


 if (!IsContentFreeTest() && (!ValidateCharacterSetup(PlayerPawn) || !FoundPlayerCombatant->HasValidIdentity()
  || FoundPlayerCombatant->Persistence!=ECombatantPersistence::UniquePartyMember))
 { UE_LOG(LogBattleCoordinator,Warning,TEXT("Rejected battle: configure a unique party profile, valid executors and party identity before entering combat."));return false; }
	// Validate the complete roster before mutating context or spending resources.
	TSet<AActor*> UniqueActors;
	UniqueActors.Add(PlayerPawn);
	TArray<APawn*> Allies{PlayerPawn};
	for (APawn* Ally : AdditionalAllies)
	{
		if (!IsValid(Ally) || Ally->GetWorld()!=GetWorld() || UniqueActors.Contains(Ally)) { return false; }
		auto* Stats=Ally->FindComponentByClass<UCombatantComponent>();
		if (!Stats || (!IsContentFreeTest() && (!ValidateCharacterSetup(Ally) || !Stats->HasValidIdentity() || Stats->Persistence!=ECombatantPersistence::UniquePartyMember))) { return false; }
		UniqueActors.Add(Ally); Allies.Add(Ally);
	}
	for (AActor* Enemy : Enemies)
	{
		if (!IsValid(Enemy) || Enemy->GetWorld()!=GetWorld() || UniqueActors.Contains(Enemy) || !Enemy->FindComponentByClass<UCombatantComponent>()) { return false; }
		UniqueActors.Add(Enemy);
	}
	APawn* FirstLivingAlly=nullptr;
	for (APawn* Ally:Allies) { if (!Ally->FindComponentByClass<UCombatantComponent>()->IsDefeated()) { FirstLivingAlly=Ally;break; } }
	if (!FirstLivingAlly) { return false; }
 if (!PrepareBattleRules(FoundPlayerCombatant)) { return false; }
	ExplorationPlayerPawn=PlayerPawn;
 ActiveEncounterSource = EncounterSource;
	ActivePlayerPawn = PlayerPawn;
	ActiveEnemyActor = EnemyActor;
	PlayerCombatant = FoundPlayerCombatant;
	EnemyCombatant = FoundEnemyCombatant;
	EncounterRecord.Result = FBattleResult();
	EncounterRecord.bReady = false;
	BattleAllies.Reset(); BattleEnemies.Reset(); TurnOrder.Reset(); TurnIndex=0; BattleRound=1;
	for (APawn* Ally : Allies) { BattleAllies.Add(Ally); TurnOrder.Add(Ally); CaptureParticipant(Ally,true); }
	for (AActor* Enemy : Enemies) { BattleEnemies.Add(Enemy); TurnOrder.Add(Enemy); CaptureParticipant(Enemy,false); }
	TurnIndex=TurnOrder.IndexOfByKey(FirstLivingAlly);
	ActivePlayerPawn=FirstLivingAlly;PlayerCombatant=FirstLivingAlly->FindComponentByClass<UCombatantComponent>();
	for (AActor* Enemy : Enemies) { FBattleFacingRestore::Face(Enemy,PlayerPawn); }
	for (APawn* Ally : Allies) { FBattleFacingRestore::Face(Ally,EnemyActor); }
 TurnTransitionDelay=ActiveRules->TurnTransitionDelay;
 Resources.ItemStocks.Reset();
 for (const auto& Stock:ActiveRules->InitialItems) { Resources.ItemStocks.Add(Stock.Item->ItemId,Stock.Quantity); }
 Resources.RemainingPotions=GetRemainingItems(GetDefaultItem());
 PotionHealing=GetDefaultItem()?GetDefaultItem()->HealingAmount:0;
#if WITH_DEV_AUTOMATION_TESTS
 if (TestTurnTransitionDelay>=0) { TurnTransitionDelay=TestTurnTransitionDelay; }
 MeleeDamage=FoundPlayerCombatant->PlayerCommandData && FoundPlayerCombatant->PlayerCommandData->IsValidConfiguration()?FoundPlayerCombatant->PlayerCommandData->MeleeDamage:15;
#endif
	Resources.bPowerStrikeUsed = false;
	Resources.ActionUses.Reset();
	EncounterRecord.Entry.Resources = Resources;
	SetDefenseResult(EBattleDefenseResult::None);

	++BattleSerial;
	FBattleFacingRestore::Face(PlayerPawn, EnemyActor);
	FBattleFacingRestore::Face(EnemyActor, PlayerPawn);
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

	BattlePresentation = GetBattlePlayerController() ? GetBattlePlayerController()->FindComponentByClass<UBattlePresentationComponent>() : nullptr;
	const float IntroDuration = BattlePresentation.IsValid() ? BattlePresentation->BeginBattleIntro(this, BattleSerial) : 0;
	if (IntroDuration > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(IntroTimer,
			FTimerDelegate::CreateUObject(this, &ThisClass::NotifyBattleIntroFinished, BattleSerial), IntroDuration + 1, false);
		return true;
	}
	// Headless tests and actors without camera presentation keep the immediate intro.
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

void UBattleCoordinator::NotifyBattleIntroFinished(int64 BattleId)
{
	if (BattleId != BattleSerial || CurrentState != EBattleState::BattleIntro || IsBattleOperationBusy()) { return; }
	GetWorld()->GetTimerManager().ClearTimer(IntroTimer);
	if (!HasValidActiveBattle()) { ReturnToExploration(); return; }
	TryTransitionTo(EBattleState::PlayerCommand);
}

bool UBattleCoordinator::ApplyCombatEffect(UCombatantComponent* Target, float Amount, ECombatNumberKind Kind)
{
	if (!IsValid(Target) || Target->IsDefeated() || !FMath::IsFinite(Amount) || Amount<0) { return false; }
	const float Before=Target->GetCurrentHealth();
	const bool bHealing=Kind==ECombatNumberKind::Healing;
	if (Amount>0 && !(bHealing && Before>=Target->GetMaxHealth())
		&& !(bHealing ? Target->RestoreHealth(Amount) : Target->ApplyDamage(Amount))) { return false; }
	FCombatNumber Number;
	Number.Target=Target->GetOwner();
	Number.Amount=FMath::Abs(Target->GetCurrentHealth()-Before);
	Number.Kind=Kind;
	CombatNumberApplied.Broadcast(Number);
	CombatNumberAppliedBlueprint.Broadcast(Number);
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
	if ((NewState == EBattleState::Victory || NewState == EBattleState::Defeat)
		&& CurrentState != EBattleState::Exploration && CurrentState != EBattleState::ReturnToField
		&& CurrentState != EBattleState::Victory && CurrentState != EBattleState::Defeat) { return true; }

	switch (CurrentState)
	{
	case EBattleState::Exploration:
		return NewState == EBattleState::BattleIntro;

	case EBattleState::BattleIntro:
		return NewState == EBattleState::PlayerCommand
			|| NewState == EBattleState::ReturnToField;

	case EBattleState::PlayerCommand:
		return NewState == EBattleState::PlayerAction
			|| (NewState==EBattleState::TurnEnd && PlayerCombatant.IsValid() && PlayerCombatant->IsDefeated())
			|| NewState == EBattleState::ReturnToField;

	case EBattleState::PlayerAction:
		return NewState == EBattleState::TurnEnd
			|| NewState == EBattleState::ReturnToField;

	case EBattleState::EnemyTelegraph:
		return NewState == EBattleState::EnemyAction
			|| (NewState==EBattleState::TurnEnd && ((PlayerCombatant.IsValid() && PlayerCombatant->IsDefeated()) || (EnemyCombatant.IsValid() && EnemyCombatant->IsDefeated())))
			|| NewState == EBattleState::ReturnToField;

	case EBattleState::EnemyAction:
		return NewState == EBattleState::DefenseWindow
			|| NewState == EBattleState::TurnEnd
			|| NewState == EBattleState::ReturnToField;

	case EBattleState::DefenseWindow:
		return NewState == EBattleState::EnemyAction
			|| NewState == EBattleState::TurnEnd
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

void UBattleCoordinator::ClearBattleTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnemyAction.TelegraphTimer);
		World->GetTimerManager().ClearTimer(EnemyAction.HitTimer);
		World->GetTimerManager().ClearTimer(TurnTransitionTimer);
	}
	EnemyAction.TelegraphTimer.Invalidate();
	EnemyAction.HitTimer.Invalidate();
}

bool UBattleCoordinator::ResolveTurnEnd(const ECompletedAction CompletedAction, bool bAfterDelay)
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

	if (TryFinishBattle()) { return true; }

	if (!bAfterDelay && TurnTransitionDelay>0)
	{
		if (GetWorld()->GetTimerManager().IsTimerActive(TurnTransitionTimer)) { return true; }
		const int64 ExpectedBattle=BattleSerial;
		GetWorld()->GetTimerManager().SetTimer(TurnTransitionTimer,FTimerDelegate::CreateWeakLambda(this,[this,ExpectedBattle,CompletedAction]()
		{
			if (ExpectedBattle==BattleSerial && CurrentState==EBattleState::TurnEnd) { ResolveTurnEnd(CompletedAction,true); }
		}),TurnTransitionDelay,false);
		return true;
	}
	return AdvanceCombatantTurn();
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

// 참가자의 체력 통지를 해제한 뒤 연출·타이머·전투 참조를 정리한다.
void UBattleCoordinator::ClearBattleContext()
{
	if (GetBattlePlayerController())
	{
		if (auto* Presentation = GetBattlePlayerController()->FindComponentByClass<UBattleResultPresentationComponent>())
		{
			Presentation->RestoreWorld();
		}
	}
	ClearParticipants();
	EncounterRecord.bReady = false;
	BattleAllies.Reset();
	BattleEnemies.Reset(); TurnOrder.Reset(); ExplorationPlayerPawn.Reset(); TurnIndex=0; BattleRound=1;
	PendingAction = nullptr;
	PendingItem = nullptr;
	PlayerAction.Item = nullptr;
	ActiveRules = nullptr;
	Resources.ItemStocks.Reset();
	Resources.ActionUses.Reset();
	EncounterRecord.Entry.Resources = FBattleResources();
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(TargetConfirmationTimer); }
	bConfirmingTarget = false;
	bSelectingTarget = false;
	Targets.Reset();
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(IntroTimer); }
	if (BattlePresentation.IsValid()) { BattlePresentation->ReleaseParryFeedback(); BattlePresentation->RestoreExplorationCamera(); }
	BattlePresentation.Reset();
	ClearPlayerAction();
	ClearEnemyAction();
	EnemyAction.CounterAction=nullptr;
	ClearBattleTimers();
	ClearEnemyPattern();
	ActiveEncounterSource.Reset();
	ActivePlayerPawn.Reset();
	ActiveEnemyActor.Reset();
	PlayerCombatant.Reset();
	EnemyCombatant.Reset();
	Resources.bPowerStrikeUsed = false;
	Resources.RemainingPotions = 0;
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

bool UBattleCoordinator::IsContentFreeTest() const
{
#if WITH_DEV_AUTOMATION_TESTS
 return bContentFreeTest;
#else
 return false;
#endif
}
bool UBattleCoordinator::ConfigureRules(UBattleRulesData* Rules)
{
 if (CurrentState!=EBattleState::Exploration || !Rules || !Rules->IsValidConfiguration()) { return false; }
 ConfiguredRules=Rules;return true;
}
bool UBattleCoordinator::PrepareBattleRules(UCombatantComponent* Player)
{
 UBattleRulesData* Rules=ConfiguredRules;
 if (!Rules)
 {
  if (const auto* Mode=Cast<AExpeditionPortfolioGameMode>(GetWorld()->GetAuthGameMode())) { Rules=Mode->BattleRules; }
 }
#if WITH_DEV_AUTOMATION_TESTS
 if (!Rules && bContentFreeTest)
 {
  Rules=NewObject<UBattleRulesData>(this);
  auto* Item=NewObject<UCombatItemData>(Rules);Item->ItemId=TEXT("Test.Potion");Item->DisplayName=FText::FromString(TEXT("Test Potion"));
  const auto* Commands=Player?Player->PlayerCommandData.Get():nullptr;
  if (Commands && !Commands->IsValidConfiguration()) { Commands=nullptr; }
  Item->HealingAmount=Commands?Commands->PotionHealing:30;
  FCombatItemStock Stock;Stock.Item=Item;Stock.Quantity=Commands?Commands->PotionsPerEncounter:3;
  Rules->InitialItems.Add(Stock);Rules->TurnTransitionDelay=Commands?Commands->TurnTransitionDelay:1;
 }
#endif
 if (!Rules || !Rules->IsValidConfiguration())
 { UE_LOG(LogBattleCoordinator,Warning,TEXT("Rejected battle: common Battle Rules are missing or invalid."));return false; }
 ActiveRules=Rules;return true;
}
bool UBattleCoordinator::ValidateCharacterSetup(APawn* Player) const
{
 const auto* Stats=Player?Player->FindComponentByClass<UCombatantComponent>():nullptr;
 const auto* Profile=Stats?Stats->CharacterActions.Get():nullptr;
 if (!Profile || !Profile->IsValidConfiguration()) { return false; }
 TArray<UCharacterActionData*> Actions;Actions.Add(Profile->BasicAttack);Actions.Add(Profile->ParryCounter);
 for (UCharacterActionData* Skill:Profile->Skills) { Actions.Add(Skill); }
 TInlineComponentArray<UActorComponent*> Components(Player);
 for (UCharacterActionData* Action:Actions)
 {
  bool bFound=false;
  for (UActorComponent* Component:Components)
  {
   if ((!Action->ExecutorComponentName.IsNone() && Component->GetFName()!=Action->ExecutorComponentName) || !Component->GetClass()->ImplementsInterface(UCharacterActionExecutor::StaticClass())) { continue; }
   const float Duration=ICharacterActionExecutor::Execute_GetCharacterActionDuration(Component,Action,Action==Profile->ParryCounter);
   if (ICharacterActionExecutor::Execute_SupportsCharacterAction(Component,Action) && FMath::IsFinite(Duration) && Duration>0 && Duration<60) { bFound=true;break; }
  }
  if (!bFound) { return false; }
 }
 return true;
}
