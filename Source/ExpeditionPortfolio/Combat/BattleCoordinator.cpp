// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BattleCoordinator.h"
#include "Combat/ParryCounterProvider.h"
#include "Combat/CharacterCombatData.h"
#include "Combat/BattleRulesData.h"
#include "Combat/CombatItemData.h"
#include "UI/ItemPresentationComponent.h"
#include "ExpeditionPortfolioGameMode.h"
#include "Combat/CharacterActionExecutor.h"
#include "UI/DefensePresentationComponent.h"

#include "Combat/CombatantComponent.h"
#include "Combat/ActionQTESubsystem.h"
#include "Combat/PlayerCommandData.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/PlayerUtilityPresentationComponent.h"
#include "UI/MagicPresentationComponent.h"
#include "UI/MagicActionData.h"
#endif
#include "UI/EnemyAttackPresentationComponent.h"
#include "UI/BattlePresentationComponent.h"
#include "UI/BattleResultPresentationComponent.h"
#include "Exploration/Encounters/BattleEncounterTrigger.h"
#include "Animation/AnimMontage.h"
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
	ResultSnapshot = FBattleResult();
	bResultReady = false;
	BattleAllies.Reset(); BattleEnemies.Reset(); TurnOrder.Reset(); TurnIndex=0; BattleRound=1;
	for (APawn* Ally : Allies) { BattleAllies.Add(Ally); TurnOrder.Add(Ally); CaptureParticipant(Ally,true); }
	for (AActor* Enemy : Enemies) { BattleEnemies.Add(Enemy); TurnOrder.Add(Enemy); CaptureParticipant(Enemy,false); }
	TurnIndex=TurnOrder.IndexOfByKey(FirstLivingAlly);
	ActivePlayerPawn=FirstLivingAlly;PlayerCombatant=FirstLivingAlly->FindComponentByClass<UCombatantComponent>();
	for (AActor* Enemy : Enemies) { FBattleFacingRestore::Face(Enemy,PlayerPawn); }
	for (APawn* Ally : Allies) { FBattleFacingRestore::Face(Ally,EnemyActor); }
 TurnTransitionDelay=ActiveRules->TurnTransitionDelay;
 ItemStocks.Reset();
 for (const auto& Stock:ActiveRules->InitialItems) { ItemStocks.Add(Stock.Item->ItemId,Stock.Quantity); }
 EntryItemStocks=ItemStocks;
 RemainingPotions=GetRemainingItems(GetDefaultItem());
 PotionHealing=GetDefaultItem()?GetDefaultItem()->HealingAmount:0;
#if WITH_DEV_AUTOMATION_TESTS
 if (TestTurnTransitionDelay>=0) { TurnTransitionDelay=TestTurnTransitionDelay; }
 MeleeDamage=FoundPlayerCombatant->PlayerCommandData && FoundPlayerCombatant->PlayerCommandData->IsValidConfiguration()?FoundPlayerCombatant->PlayerCommandData->MeleeDamage:15;
#endif
 bPowerStrikeUsed=false;ActionUses.Reset();EntryActionUses=ActionUses;
 EntryPotions=RemainingPotions;bEntryPowerStrikeUsed=bPowerStrikeUsed;
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
	bResultReady = false;
	BattleAllies.Reset();
	BattleEnemies.Reset(); TurnOrder.Reset(); ExplorationPlayerPawn.Reset(); TurnIndex=0; BattleRound=1;
	PendingAction=nullptr;PendingItem=nullptr;PlayerAction.Item=nullptr;ActiveRules=nullptr;ItemStocks.Reset();EntryItemStocks.Reset();ActionUses.Reset(); EntryActionUses.Reset();
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(TargetConfirmationTimer); }
	bConfirmingTarget = false;
	bSelectingTarget = false;
	Targets.Reset();
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(IntroTimer); }
	if (BattlePresentation.IsValid()) { BattlePresentation->ReleaseParryFeedback(); BattlePresentation->RestoreExplorationCamera(); }
	BattlePresentation.Reset();
	PlayerFacingRestore.Restore();
	EnemyFacingRestore.Restore();
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
	bPowerStrikeUsed = false;
	RemainingPotions = 0;
}

void UBattleCoordinator::CaptureParticipant(AActor* Actor, bool bAlly)
{
	if (!Actor || Participants.ContainsByPredicate([Actor](const FParticipantSnapshot& Entry) { return Entry.Actor.Get()==Actor; })) { return; }
	auto* Health = Actor->FindComponentByClass<UCombatantComponent>();
	if (!Health) { return; }
	FParticipantSnapshot Entry;
	Entry.Actor=Actor; Entry.Combatant=Health; Entry.Health=Health->GetCurrentHealth();
	Entry.Transform=Actor->GetActorTransform(); Entry.bAlly=bAlly;
	Entry.FacingRestore.Capture(Actor);
	Entry.HealthHandle=Health->OnHealthChanged().AddWeakLambda(this,[this,bAlly](float Before,float After)
	{
		if (CurrentState==EBattleState::Victory || CurrentState==EBattleState::Defeat) { return; }
		const float Damage=FMath::Max(0.f,Before-After);
		if (bAlly) { ResultSnapshot.DamageReceived+=Damage; } else { ResultSnapshot.DamageDealt+=Damage; }
		// External damage is checked after its broadcast stack unwinds. Normal hit paths check immediately.
		if (After<=0 && !ResultCheckTicker.IsValid())
		{
			ResultCheckTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float)
			{
				if (IsBattleOperationBusy()) { return true; }
				ResultCheckTicker.Reset();
				HandleParticipantDefeat();
				return false;
			}));
		}
	});
	Participants.Add(Entry);
}

void UBattleCoordinator::ClearParticipants()
{
	if (ResultCheckTicker.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(ResultCheckTicker); ResultCheckTicker.Reset(); }
	for (auto& Entry:Participants)
	{
		Entry.FacingRestore.Restore();
		if (Entry.Combatant.IsValid()) { Entry.Combatant->OnHealthChanged().Remove(Entry.HealthHandle); }
	}
	Participants.Reset();
}

bool UBattleCoordinator::TryFinishBattle()
{
	if (CurrentState==EBattleState::Victory || CurrentState==EBattleState::Defeat) { return true; }
	if (CurrentState==EBattleState::Exploration || CurrentState==EBattleState::ReturnToField
		|| !PlayerCombatant.IsValid() || !EnemyCombatant.IsValid()) { return false; }
	const bool bAllEnemiesDefeated=!HasLivingSide(false);
	const bool bDefeat=!HasLivingSide(true);
	if (!bDefeat && !bAllEnemiesDefeated) { return false; }
	ResultSnapshot.bVictory=!bDefeat;
	ResultSnapshot.Party.Reset();
	for (const auto& Entry:Participants)
	{
		if (!Entry.bAlly || !Entry.Combatant.IsValid()) { continue; }
		FBattlePartyResult Member;
		Member.Name=Entry.Combatant->CombatDisplayName;
		if (Member.Name.IsEmpty() || Member.Name.ToString()==TEXT("적"))
		{
			Member.Name=FText::FromString(Entry.Actor.Get()==ActivePlayerPawn.Get()?TEXT("플레이어"):TEXT("동료"));
		}
		Member.Health=Entry.Combatant->GetCurrentHealth(); Member.MaxHealth=Entry.Combatant->GetMaxHealth();
		ResultSnapshot.Party.Add(Member);
	}
	ClearBattleTimers();
	auto& Timers=GetWorld()->GetTimerManager();
	Timers.ClearTimer(IntroTimer); Timers.ClearTimer(TargetConfirmationTimer); Timers.ClearTimer(PlayerAction.TimeoutTimer);
	Timers.ClearTimer(EnemyAction.WindowOpenTimer); Timers.ClearTimer(EnemyAction.TimeoutTimer); Timers.ClearTimer(EnemyAction.CounterTimer);
	GetWorld()->GetSubsystem<UActionQTESubsystem>()->CancelQTE(PlayerAction.QTESessionId);
	bSelectingTarget=false; bConfirmingTarget=false;
	if (!TryTransitionTo(bDefeat?EBattleState::Defeat:EBattleState::Victory)) { return false; }
	// No local presentation (automation/server) still exposes the same explicit result decisions.
	if (!GetBattlePlayerController() || !GetBattlePlayerController()->FindComponentByClass<UBattleResultPresentationComponent>())
	{
		NotifyResultReady();
	}
	UE_LOG(LogBattleCoordinator,Log,TEXT("Battle result captured: Victory=%d Dealt=%.1f Received=%.1f Parries=%d Dodges=%d"),
		ResultSnapshot.bVictory,ResultSnapshot.DamageDealt,ResultSnapshot.DamageReceived,ResultSnapshot.SuccessfulParries,ResultSnapshot.SuccessfulDodges);
	return true;
}

void UBattleCoordinator::NotifyResultReady()
{
	if (CurrentState==EBattleState::Victory || CurrentState==EBattleState::Defeat) { bResultReady=true; }
}

bool UBattleCoordinator::ContinueAfterVictory()
{
	if (!bResultReady || IsBattleOperationBusy() || CurrentState!=EBattleState::Victory) { return false; }
	TGuardValue<bool> ActionGuard(bActionInProgress,true);
	TWeakObjectPtr<ABattleEncounterTrigger> Trigger=Cast<ABattleEncounterTrigger>(ActiveEncounterSource.Get());
	TArray<TWeakObjectPtr<AActor>> DefeatedEnemies;
	for (const auto& Member:Participants)
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

bool UBattleCoordinator::RestoreEncounter(bool bRetry)
{
	if (!bResultReady || IsBattleOperationBusy() || CurrentState!=EBattleState::Defeat
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
	const auto Entry=Participants;
	const int32 Potions=EntryPotions; const bool bUsed=bEntryPowerStrikeUsed; const auto Uses=EntryActionUses;const auto Stocks=EntryItemStocks;
	if (!ReturnToExploration()) { return false; }
	for (const auto& Member:Entry)
	{
		if (Member.Actor.IsValid()) { Member.Actor->SetActorTransform(Member.Transform,false,nullptr,ETeleportType::TeleportPhysics); }
		if (Member.Combatant.IsValid()) { Member.Combatant->RestoreBattleHealth(Member.Health); }
	}
	if (!bRetry)
	{
		Player->SetActorLocation(RetreatLocation,false,nullptr,ETeleportType::TeleportPhysics);
		Trigger->RearmAfterRetreat();
		return true;
	}
	if (!RequestGroupBattleStart(Source,Player,Enemies,Allies)) { return false; }
	ActionUses=Uses; EntryActionUses=Uses;ItemStocks=Stocks;EntryItemStocks=Stocks;
	RemainingPotions=Potions; EntryPotions=Potions; bPowerStrikeUsed=bUsed; bEntryPowerStrikeUsed=bUsed;
	for (const auto& Member:Entry)
	{
		if (Member.bAlly && Member.Actor.IsValid() && Member.Actor.Get()!=Player)
		{
			BattleAllies.AddUnique(Member.Actor); CaptureParticipant(Member.Actor.Get(),true);
		}
	}
	return true;
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
	// Observers restore the shoulder view now; commands remain blocked until the camera settles.
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
