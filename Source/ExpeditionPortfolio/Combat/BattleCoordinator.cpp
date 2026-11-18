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
	if (BattleId != BattleSerial || CurrentState != EBattleState::BattleIntro || bTransitionInProgress || bActionInProgress) { return; }
	GetWorld()->GetTimerManager().ClearTimer(IntroTimer);
	if (!HasValidActiveBattle()) { ReturnToExploration(); return; }
	TryTransitionTo(EBattleState::PlayerCommand);
}

void UBattleCoordinator::SetPlayerActionPhase(EPlayerActionPhase Phase)
{
	if (PlayerActionPhase == Phase) { return; }
	PlayerActionPhase = Phase;
	PlayerPhaseChanged.Broadcast(Phase);
}

void UBattleCoordinator::BeginPlayerPreparation(int64 ActionId, float Duration, float SuccessDuration, float Multiplier, bool bTimingEnabled, const UActionQTEPatternData* Pattern)
{
	if (!ActionId || ActionId != ActivePlayerActionId || CurrentState != EBattleState::PlayerAction) { return; }
	SetPlayerActionPhase(EPlayerActionPhase::Preparing);
	if (bTimingEnabled && !QTESessionId)
	{
		auto* QTE = GetWorld()->GetSubsystem<UActionQTESubsystem>();
		QTESessionId = Pattern ? QTE->BeginPattern(Pattern) : QTE->BeginQTE(Duration, SuccessDuration, Multiplier);
		if (!QTESessionId) { UE_LOG(LogBattleCoordinator, Warning, TEXT("Invalid or busy QTE; preparing without bonus.")); }
		AttackTimingChanged.Broadcast();
	}
}
void UBattleCoordinator::NotifyPlayerPreparationFinished(int64 ActionId)
{
	if (!ActionId || ActionId != ActivePlayerActionId || CurrentState != EBattleState::PlayerAction || PlayerActionPhase != EPlayerActionPhase::Preparing) { return; }
	GetWorld()->GetSubsystem<UActionQTESubsystem>()->FinishWindow(QTESessionId);
	SetPlayerActionPhase(EPlayerActionPhase::Executing);
}
bool UBattleCoordinator::RequestAttackTimingInput()
{
	if (bActionInProgress || bTransitionInProgress || CurrentState != EBattleState::PlayerAction || !ActivePlayerActionId
		|| PlayerActionPhase != EPlayerActionPhase::Preparing || !QTESessionId) { return false; }
	RefreshAreaPrimaryTarget();
	if (!HasValidActiveBattle()) { ReturnToExploration(); return false; }
	TGuardValue<bool> Guard(bActionInProgress, true);
	return GetWorld()->GetSubsystem<UActionQTESubsystem>()->SubmitInput(QTESessionId);
}
FAttackTimingView UBattleCoordinator::GetAttackTimingView() const
{
	const auto View = GetWorld()->GetSubsystem<UActionQTESubsystem>()->GetView();
	return QTESessionId && QTESessionId == View.SessionId ? View : FAttackTimingView();
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

	return !bEnemyHitResolved && PendingDefenseResult == EBattleDefenseResult::None
		&& GetEnemyHitTimeRemaining() > 0.0f
		&& (Command == EBattleDefenseCommand::Parry || Command == EBattleDefenseCommand::Dodge);
}

float UBattleCoordinator::GetEnemyHitTimeRemaining() const
{
	return GetWorld() && ActiveEnemyActionId && !bEnemyHitResolved
		? FMath::Max(0.0f, GetWorld()->GetTimerManager().GetTimerRemaining(DefenseWindowTimerHandle)) : 0.0f;
}

bool UBattleCoordinator::RequestDefenseCommand(const EBattleDefenseCommand Command)
{
	if (bTransitionInProgress || bActionInProgress)
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
	if (Remaining <= 0.0f || bEnemyHitResolved || PendingDefenseResult != EBattleDefenseResult::None)
	{
		LogRejectedCommand(TEXT("RequestDefenseCommand"), TEXT("hit already resolved or defense input already submitted"));
		return false;
	}

	PendingDefenseResult = Command == EBattleDefenseCommand::Dodge
		? EBattleDefenseResult::DodgeSuccess
		: Remaining <= ActiveEnemyPattern.ParrySuccessDuration + KINDA_SMALL_NUMBER
			? EBattleDefenseResult::ParrySuccess
			: EBattleDefenseResult::Failed;
	UE_LOG(LogBattleCoordinator, Log, TEXT("Defense input buffered: Remaining=%.3f Result=%s"),
		Remaining, *UEnum::GetValueAsString(PendingDefenseResult));
	TGuardValue<bool> ActionGuard(bActionInProgress, true);
	DefenseCommandAccepted.Broadcast(Command, Remaining);
	return true;
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

bool UBattleCoordinator::ExecutePlayerCommand(const EBattlePlayerCommand Command)
{
	UCharacterActionData* Selected=IsItemCommand(Command)?nullptr:ResolveCommandAction(Command);
 UCombatItemData* SelectedItem=IsItemCommand(Command)?(Command==EBattlePlayerCommand::UsePotion?GetDefaultItem():PendingItem?PendingItem.Get():GetDefaultItem()):nullptr;
	const float Damage = Selected?Selected->BaseDamage:SelectedItem?SelectedItem->HealingAmount
		: Command == EBattlePlayerCommand::MeleeAttack ? MeleeDamage : GetMagicDamage(Command);
	const auto Scope=Selected?Selected->TargetScope:ECharacterActionTargetScope::SingleEnemy;
	TArray<TWeakObjectPtr<AActor>> AcceptedTargets;
	if (!SelectedItem)
	{
		if (Scope==ECharacterActionTargetScope::AllEnemies)
		{
			for (const auto& Enemy:BattleEnemies) { if (IsRegisteredEnemy(Enemy.Get())) { AcceptedTargets.Add(Enemy); } }
		}
		else if (IsRegisteredEnemy(ActiveEnemyActor.Get())) { AcceptedTargets.Add(ActiveEnemyActor); }
		if (AcceptedTargets.IsEmpty()) { return false; }
	}



	TGuardValue<bool> ActionGuard(bActionInProgress, true);

	if (!TryTransitionTo(EBattleState::PlayerAction))
	{
		return false;
	}

 ActiveAction=Selected;
	ActiveTargetScope=Scope;
	ActionTargets=MoveTemp(AcceptedTargets);
 if (Selected) { ++ActionUses.FindOrAdd(ActivePlayerPawn.Get()).FindOrAdd(Selected->ActionId); }
 else if (Command==EBattlePlayerCommand::PowerStrike) { bPowerStrikeUsed=true; }
 ActivePlayerActionId = ++NextPlayerActionId;
	QTESessionId = 0;
	AttackTimingChanged.Broadcast();
	FBattleFacingRestore::Face(ActivePlayerPawn.Get(), ActiveEnemyActor.Get());
	FBattleFacingRestore::Face(ActiveEnemyActor.Get(), ActivePlayerPawn.Get());
	ActivePlayerCommand = Command;
 ActiveItem=SelectedItem;
 if (SelectedItem) { --ItemStocks.FindChecked(SelectedItem->ItemId);RemainingPotions=GetRemainingItems(GetDefaultItem()); }
	PendingPlayerDamage = Damage;
	bPlayerHitApplied = false;
 if (Selected)
 {
  ActionPresentation=FindActionExecutor(Selected);
  if (ActionPresentation.IsValid())
  {
   const float Expected=ICharacterActionExecutor::Execute_GetCharacterActionDuration(ActionPresentation.Get(),Selected,false);
   if (FMath::IsFinite(Expected) && Expected>0 && Expected<60
    && ICharacterActionExecutor::Execute_StartCharacterAction(ActionPresentation.Get(),Selected,ActivePlayerActionId,ActiveEnemyActor.Get(),false))
   {
    if (PlayerActionPhase!=EPlayerActionPhase::Preparing) { SetPlayerActionPhase(EPlayerActionPhase::Executing); }
    GetWorld()->GetTimerManager().SetTimer(PlayerActionTimerHandle,FTimerDelegate::CreateUObject(this,&ThisClass::HandlePlayerActionTimeout,ActivePlayerActionId),Expected+1,false);
    return true;
   }
  }
  UE_LOG(LogBattleCoordinator,Warning,TEXT("Character action presentation unavailable: %s"),*GetNameSafe(Selected));
  bActionInProgress=false;
  return NotifyPlayerActionFinished(ActivePlayerActionId,!IsContentFreeTest());
 }
 if (SelectedItem)
 {
  SetPlayerActionPhase(EPlayerActionPhase::Executing);
  ItemPresentation=ActivePlayerPawn->FindComponentByClass<UItemPresentationComponent>();
  if (ItemPresentation.IsValid())
  {
   const float Expected=ItemPresentation->GetExpectedDuration();
   if (FMath::IsFinite(Expected) && Expected>0 && Expected<60 && ItemPresentation->BeginUse(SelectedItem,ActivePlayerActionId,ItemTarget.IsValid()?ItemTarget.Get():ActivePlayerPawn.Get()))
   {
    GetWorld()->GetTimerManager().SetTimer(PlayerActionTimerHandle,FTimerDelegate::CreateUObject(this,&ThisClass::HandlePlayerActionTimeout,ActivePlayerActionId),Expected+1,false);
    return true;
   }
  }
  bActionInProgress=false;return NotifyPlayerActionFinished(ActivePlayerActionId,!IsContentFreeTest());
 }
 if (!IsContentFreeTest()) { bActionInProgress=false;return NotifyPlayerActionFinished(ActivePlayerActionId,true); }

#if WITH_DEV_AUTOMATION_TESTS
 if (Command == EBattlePlayerCommand::MeleeAttack || IsItemCommand(Command))
	{
		SetPlayerActionPhase(EPlayerActionPhase::Executing);
		UtilityPresentation = ActivePlayerPawn->FindComponentByClass<UPlayerUtilityPresentationComponent>();
		if (UtilityPresentation.IsValid())
		{
			const float Expected = UtilityPresentation->GetExpectedDuration(Command);
			if (FMath::IsFinite(Expected) && Expected > 0 && Expected < 60
				&& UtilityPresentation->BeginAction(Command, ActivePlayerActionId, IsItemCommand(Command) ? (ItemTarget.IsValid()?ItemTarget.Get():ActivePlayerPawn.Get()) : ActiveEnemyActor.Get()))
			{
				GetWorld()->GetTimerManager().SetTimer(PlayerActionTimerHandle,
					FTimerDelegate::CreateUObject(this, &ThisClass::HandlePlayerActionTimeout, ActivePlayerActionId), Expected + 1, false);
				return true;
			}
			UE_LOG(LogBattleCoordinator, Warning, TEXT("Utility presentation unavailable; resolving accepted command without animation."));
		}
		bActionInProgress = false;
		return NotifyPlayerActionFinished(ActivePlayerActionId, false);
	}
	PlayerPresentation = ActivePlayerPawn->FindComponentByClass<UMagicPresentationComponent>();
	if (PlayerPresentation.IsValid())
	{
		const float Expected = PlayerPresentation->GetExpectedDuration(Command);
		if (FMath::IsFinite(Expected) && Expected > 0.0f && Expected < 60.0f
			&& PlayerPresentation->BeginMagicAction(Command, ActivePlayerActionId, ActiveEnemyActor.Get()))
		{
			if (PlayerActionPhase != EPlayerActionPhase::Preparing) { SetPlayerActionPhase(EPlayerActionPhase::Executing); }
			GetWorld()->GetTimerManager().SetTimer(PlayerActionTimerHandle,
				FTimerDelegate::CreateUObject(this, &ThisClass::HandlePlayerActionTimeout, ActivePlayerActionId), Expected + 1.0f, false);
			return true;
		}
		UE_LOG(LogBattleCoordinator, Warning, TEXT("Magic presentation failed; resolving accepted command without animation."));
	}
	// Keep the headless/no-presentation path usable for core tests and prototype pawns.
	// Public signal handlers normally reject reentrancy; release the command guard for this internal fallback.
	bActionInProgress = false;
	return NotifyPlayerActionFinished(ActivePlayerActionId, false);
#else
 return false;
#endif
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

bool UBattleCoordinator::NotifyPlayerActionHit(const int64 ActionId)
{
	if (ActionId == 0 || ActionId != ActivePlayerActionId || CurrentState != EBattleState::PlayerAction
		|| bActionInProgress || bTransitionInProgress || bPlayerHitApplied || PlayerActionPhase == EPlayerActionPhase::Preparing)
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
	bPlayerHitApplied = true;
	const float Multiplier = GetWorld()->GetSubsystem<UActionQTESubsystem>()->GetDamageMultiplier(QTESessionId);
	const float AppliedAmount = PendingPlayerDamage * Multiplier;
	AActor* HealTarget=ItemTarget.IsExplicitlyNull()?ActivePlayerPawn.Get():ItemTarget.Get();
	if (IsItemCommand(ActivePlayerCommand) && !CanHealAlly(HealTarget)) { return false; }
	bool bApplied=false;
	if (IsItemCommand(ActivePlayerCommand))
	{
		bApplied=ApplyCombatEffect(HealTarget->FindComponentByClass<UCombatantComponent>(),AppliedAmount,ECombatNumberKind::Healing);
	}
	else
	{
		// Apply the accepted batch under one action guard. A lethal first target cannot end the batch.
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
		ActionId, *UEnum::GetValueAsString(ActivePlayerCommand), AppliedAmount);
	TryFinishBattle();
	return true;
}

bool UBattleCoordinator::NotifyPlayerActionFinished(const int64 ActionId, const bool bInterrupted)
{
	if (ActionId == 0 || ActionId != ActivePlayerActionId || CurrentState != EBattleState::PlayerAction
		|| bActionInProgress || bTransitionInProgress)
	{
		return false;
	}
	// Missing hit notify: complete the accepted command once at a normal animation end.
	// Interrupted/timeout actions do not manufacture a hit; the consumed skill stays consumed.
	if (!bInterrupted && !bPlayerHitApplied)
	{

#if WITH_DEV_AUTOMATION_TESTS
		if (PlayerPresentation.IsValid())
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
	if (ActionId != ActivePlayerActionId || CurrentState != EBattleState::PlayerAction) { return; }
	UE_LOG(LogBattleCoordinator, Warning, TEXT("Magic action completion timed out; cancelling presentation."));
	NotifyPlayerActionFinished(ActionId, true);
}

void UBattleCoordinator::ClearPlayerAction()
{
	ActionTargets.Reset();
	ActiveTargetScope=ECharacterActionTargetScope::SingleEnemy;
	ItemTarget.Reset();
	if (GetWorld()) { GetWorld()->GetSubsystem<UActionQTESubsystem>()->CancelQTE(QTESessionId); }
	QTESessionId = 0;
	SetPlayerActionPhase(EPlayerActionPhase::None);
	ActivePlayerActionId = 0;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(PlayerActionTimerHandle); }
	if (ItemPresentation.IsValid()) { ItemPresentation->StopPresentation(); }
	ItemPresentation.Reset();ActiveItem=nullptr;
	if (ActionPresentation.IsValid()) { ICharacterActionExecutor::Execute_StopCharacterAction(ActionPresentation.Get()); }
	ActionPresentation.Reset(); ActiveAction=nullptr;

#if WITH_DEV_AUTOMATION_TESTS
	if (PlayerPresentation.IsValid()) { PlayerPresentation->StopPresentation(); }
	if (UtilityPresentation.IsValid()) { UtilityPresentation->StopPresentation(); }
	UtilityPresentation.Reset();
	PlayerPresentation.Reset();
#endif
	PendingPlayerDamage = 0.0f;
	bPlayerHitApplied = false;
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

FEnemyAttackPatternSnapshot UBattleCoordinator::GetActiveEnemyPattern() const
{
	FEnemyAttackPatternSnapshot Result = ActiveEnemyPattern;
	if (!bHasActiveEnemyPattern)
	{
		Result.PatternId = NAME_None;
		Result.DisplayName = FText::GetEmpty();
	}
	return Result;
}

void UBattleCoordinator::ClearEnemyPattern()
{
	bHasActiveEnemyPattern = false;
	ActiveEnemyPattern = FEnemyAttackPatternSnapshot();
}

void UBattleCoordinator::SelectEnemyPattern()
{
	const auto& Patterns = EnemyCombatant->EnemyAttackPatterns;
	for (const UEnemyAttackPatternData* Pattern : Patterns)
	{
		FString Reason;
		if (!IsValid(Pattern) || !Pattern->ValidatePattern(Reason))
		{
			UE_LOG(LogBattleCoordinator, Warning, TEXT("Enemy '%s': skipping pattern '%s': %s"),
				*GetNameSafe(ActiveEnemyActor.Get()), *GetNameSafe(Pattern),
				IsValid(Pattern) ? *Reason : TEXT("null or invalid reference"));
		}
	}
	const UEnemyAttackPatternData* Selected = UEnemyAttackPatternData::SelectPattern(Patterns, PatternRandom);
	ActiveEnemyPattern = Selected ? Selected->MakeSnapshot() : FEnemyAttackPatternSnapshot();
	bHasActiveEnemyPattern = true;
	if (!Selected)
	{
		UE_LOG(LogBattleCoordinator, Warning,
			TEXT("Enemy '%s': no selectable enemy patterns (empty, invalid, or zero weights); using BasicAttack fallback."),
			*GetNameSafe(ActiveEnemyActor.Get()));
	}
	UE_LOG(LogBattleCoordinator, Log, TEXT("Enemy pattern selected: %s, Damage=%.3f Telegraph=%.3f Defense=%.3f Parry=%.3f"),
		*ActiveEnemyPattern.PatternId.ToString(), ActiveEnemyPattern.Damage, ActiveEnemyPattern.TelegraphDuration,
		ActiveEnemyPattern.DefenseWindowDuration, ActiveEnemyPattern.ParrySuccessDuration);
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
		EnemyTelegraphTimerHandle,
		this,
		&ThisClass::HandleEnemyTelegraphFinished,
		ActiveEnemyPattern.TelegraphDuration,
		false);
	return true;
}

void UBattleCoordinator::HandleEnemyTelegraphFinished()
{
	EnemyTelegraphTimerHandle.Invalidate();
	if (CurrentState!=EBattleState::EnemyTelegraph || !HasValidActiveBattle()) { ReturnToExploration(); return; }
	if (!TryTransitionTo(EBattleState::EnemyAction)) { ReturnToExploration(); return; }
	ActiveEnemyActionId=++NextEnemyActionId;
	EnemyHitIndex=0; bAllEnemyHitsParried=true; bEnemyAttackStarted=false;
	PendingDefenseResult=EBattleDefenseResult::None; bEnemyHitResolved=false; bEnemyHitNotifyReceived=false;
	if (ActiveEnemyPattern.Hits.IsEmpty())
	{
		FEnemyAttackHit Hit; Hit.Damage=ActiveEnemyPattern.Damage; Hit.HitTime=ActiveEnemyPattern.HitTime;
		Hit.DefenseWindowDuration=ActiveEnemyPattern.DefenseWindowDuration; Hit.ParrySuccessDuration=ActiveEnemyPattern.ParrySuccessDuration;
		Hit.HitTimeOffset=ActiveEnemyPattern.HitTimeOffset; ActiveEnemyPattern.Hits.Add(Hit);
	}
	EnemyPresentation=ActiveEnemyActor->FindComponentByClass<UEnemyAttackPresentationComponent>();
	bWaitingForEnemyAnimation=ActiveEnemyPattern.AttackMontage && EnemyPresentation.IsValid();
	if (bWaitingForEnemyAnimation)
	{
		bWaitingForEnemyAnimation=EnemyPresentation->BeginAttack(ActiveEnemyPattern.AttackMontage,ActiveEnemyActionId,ActivePlayerPawn.Get());
		if (!bWaitingForEnemyAnimation && EnemyPresentation->WasTravelBlocked())
		{
			NotifyEnemyActionFinished(ActiveEnemyActionId,true);return;
		}
	}
	if (!bWaitingForEnemyAnimation) { NotifyEnemyAttackStarted(ActiveEnemyActionId); }
	if (bWaitingForEnemyAnimation)
	{
		GetWorld()->GetTimerManager().SetTimer(EnemyActionTimeoutTimerHandle,
			FTimerDelegate::CreateUObject(this,&ThisClass::HandleEnemyActionTimeout,ActiveEnemyActionId),
			EnemyPresentation->GetExpectedDuration(ActiveEnemyPattern.AttackMontage)+1.f,false);
	}
	UE_LOG(LogBattleCoordinator,Log,TEXT("Enemy combo started: Id=%lld Hits=%d"),ActiveEnemyActionId,ActiveEnemyPattern.Hits.Num());
}

void UBattleCoordinator::NotifyEnemyAttackStarted(const int64 ActionId)
{
	if (!ActionId || ActionId!=ActiveEnemyActionId || bEnemyAttackStarted || CurrentState!=EBattleState::EnemyAction) { return; }
	bEnemyAttackStarted=true;
	EnemyAttackStartedAt=GetWorld()->GetTimeSeconds();
	ScheduleEnemyHit();
}

void UBattleCoordinator::ScheduleEnemyHit()
{
	if (!ActiveEnemyPattern.Hits.IsValidIndex(EnemyHitIndex) || !ActiveEnemyActionId) { return; }
	const auto& Hit=ActiveEnemyPattern.Hits[EnemyHitIndex];
	ActiveEnemyPattern.Damage=Hit.Damage; ActiveEnemyPattern.HitTime=Hit.HitTime;
	ActiveEnemyPattern.DefenseWindowDuration=Hit.DefenseWindowDuration;
	ActiveEnemyPattern.ParrySuccessDuration=Hit.ParrySuccessDuration; ActiveEnemyPattern.HitTimeOffset=Hit.HitTimeOffset;
	PendingDefenseResult=EBattleDefenseResult::None; bEnemyHitResolved=false; bEnemyHitNotifyReceived=false;
	auto& Timers=GetWorld()->GetTimerManager();
	Timers.ClearTimer(DefenseWindowTimerHandle); Timers.ClearTimer(EnemyWindowOpenTimerHandle);
	const float Delay=FMath::Max(.001f,EnemyAttackStartedAt+Hit.HitTime-GetWorld()->GetTimeSeconds());
	Timers.SetTimer(DefenseWindowTimerHandle,this,&ThisClass::HandleDefenseWindowExpired,Delay,false);
	if (Delay>Hit.DefenseWindowDuration+KINDA_SMALL_NUMBER)
	{
		Timers.SetTimer(EnemyWindowOpenTimerHandle,this,&ThisClass::OpenEnemyDefenseWindow,Delay-Hit.DefenseWindowDuration,false);
	}
	else { OpenEnemyDefenseWindow(); }
}

void UBattleCoordinator::OpenEnemyDefenseWindow()
{
	if (CurrentState != EBattleState::EnemyAction || !ActiveEnemyActionId) { return; }
	if (!HasValidActiveBattle()) { ReturnToExploration(); return; }
	TryTransitionTo(EBattleState::DefenseWindow);
}

void UBattleCoordinator::HandleDefenseWindowExpired()
{
	if (!ActiveEnemyActionId || bEnemyHitResolved) { return; }
	if (!HasValidActiveBattle())
	{
		ReturnToExploration();
		return;
	}
	if (CurrentState == EBattleState::EnemyAction) { OpenEnemyDefenseWindow(); }
	GetWorld()->GetTimerManager().ClearTimer(DefenseWindowTimerHandle);
	TGuardValue<bool> ActionGuard(bActionInProgress, true);
	ResolveDefenseOutcome(PendingDefenseResult == EBattleDefenseResult::None ? EBattleDefenseResult::NoInput : PendingDefenseResult);
}

bool UBattleCoordinator::NotifyEnemyActionHit(const int64 ActionId, const int32 HitIndex)
{
	if (!ActionId || ActionId != ActiveEnemyActionId || HitIndex!=EnemyHitIndex || bEnemyHitResolved || bEnemyHitNotifyReceived
		|| bActionInProgress || bTransitionInProgress) { return false; }
	bEnemyHitNotifyReceived = true;
	// Offsets use the snapshotted timeline; the unshifted marker must not override them.
	if (FMath::IsNearlyZero(ActiveEnemyPattern.HitTimeOffset) && GetEnemyHitTimeRemaining() <= 0.035f)
	{
		HandleDefenseWindowExpired();
	}
	return true;
}

bool UBattleCoordinator::NotifyEnemyActionFinished(const int64 ActionId, const bool bInterrupted)
{
	if (!ActionId || ActionId != ActiveEnemyActionId || bActionInProgress || bTransitionInProgress
		|| (CurrentState != EBattleState::EnemyAction && CurrentState != EBattleState::DefenseWindow)) { return false; }
	if (!bInterrupted && !bEnemyHitResolved && ActiveEnemyPattern.Hits.Num()==1)
	{
		UE_LOG(LogBattleCoordinator, Warning, TEXT("Enemy hit marker/timer missing; resolving once at normal completion."));
		HandleDefenseWindowExpired();
		if (ActiveEnemyActionId != ActionId || CurrentState==EBattleState::Victory || CurrentState==EBattleState::Defeat) { return true; }
	}
	TGuardValue<bool> ActionGuard(bActionInProgress, true);
	if (!bEnemyHitResolved) { bAllEnemyHitsParried=false; }
	bWaitingForEnemyAnimation = false;
	return FinishEnemyAction();
}

void UBattleCoordinator::HandleEnemyActionTimeout(const int64 ActionId)
{
	if (ActionId != ActiveEnemyActionId || !ActionId) { return; }
	UE_LOG(LogBattleCoordinator, Warning, TEXT("Enemy animation completion timed out; cancelling remaining attack."));
	NotifyEnemyActionFinished(ActionId, true);
}

bool UBattleCoordinator::StartParryCounter()
{
 CounterPresentation.Reset();
 if (const auto* Profile=GetCharacterActions())
 {
  ActiveCounterAction=Profile->ParryCounter;
  if (!ActiveCounterAction || !ActiveCounterAction->IsValidAction()) { return false; }
  CounterPresentation=FindActionExecutor(ActiveCounterAction);
  if (!CounterPresentation.IsValid()) { return false; }
  const float Duration=ICharacterActionExecutor::Execute_GetCharacterActionDuration(CounterPresentation.Get(),ActiveCounterAction,true);
  if (!FMath::IsFinite(Duration) || Duration<=0 || Duration>60) { CounterPresentation.Reset(); return false; }
  PendingCounterDamage=ActiveCounterAction->BaseDamage; bParryCounterActive=true; bParryCounterHitApplied=false;
  if (EnemyPresentation.IsValid()) { EnemyPresentation->HoldForCounter(); }
  if (BattlePresentation.IsValid() && BattlePresentation->BeginParryFeedback(ActiveEnemyActionId,true)) { return true; }
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
		if (EnemyPresentation.IsValid()) { EnemyPresentation->HoldForCounter(); }
		CounterPresentation=Component; PendingCounterDamage=Damage; bParryCounterActive=true; bParryCounterHitApplied=false;
		if (IParryCounterProvider::Execute_StartCounterAction(Component,ActiveEnemyActionId,ActiveEnemyActor.Get()))
		{
			GetWorld()->GetTimerManager().SetTimer(ParryCounterTimerHandle,
				FTimerDelegate::CreateUObject(this,&ThisClass::HandleParryCounterTimeout,ActiveEnemyActionId),Duration+1.f,false);
			return true;
		}
		bParryCounterActive=false;
		IParryCounterProvider::Execute_StopCounterAction(Component); CounterPresentation.Reset();
	}
	// Content-free participants still work in logic tests. No particular spell is implied.
	const bool Applied=ApplyCombatEffect(EnemyCombatant.Get(),ParryCounterDamage,ECombatNumberKind::Damage);
	if (EnemyPresentation.IsValid() && EnemyPresentation->IsHeldForCounter()) { bWaitingForEnemyAnimation=EnemyPresentation->ReturnAfterCounter(); }
	return Applied;
#else
 return false;
#endif
}

bool UBattleCoordinator::ResolveDefenseOutcome(const EBattleDefenseResult Result)
{
	if (CurrentState!=EBattleState::DefenseWindow || bEnemyHitResolved) { return false; }
	bEnemyHitResolved=true;
	bAllEnemyHitsParried &= Result==EBattleDefenseResult::ParrySuccess;
	const bool bLastHit=EnemyHitIndex+1>=ActiveEnemyPattern.Hits.Num();
	if (Result==EBattleDefenseResult::Failed || Result==EBattleDefenseResult::NoInput)
	{
		if (!ApplyCombatEffect(PlayerCombatant.Get(),ActiveEnemyPattern.Damage,ECombatNumberKind::Damage)) { ReturnToExploration(); return false; }
	}
	if (EnemyPresentation.IsValid() && ActiveEnemyPattern.Hits.IsValidIndex(EnemyHitIndex))
	{
		EnemyPresentation->PlayHitFeedback(ActiveEnemyPattern.Hits[EnemyHitIndex],Result);
	}
	if (bLastHit && bAllEnemyHitsParried && !StartParryCounter()) { ReturnToExploration(); return false; }
	SetDefenseResult(Result);
	if (Result==EBattleDefenseResult::ParrySuccess) { ++ResultSnapshot.SuccessfulParries; }
	if (Result==EBattleDefenseResult::DodgeSuccess) { ++ResultSnapshot.SuccessfulDodges; }
	UE_LOG(LogBattleCoordinator,Log,TEXT("Enemy hit resolved: Id=%lld Hit=%d/%d Result=%s"),ActiveEnemyActionId,EnemyHitIndex+1,ActiveEnemyPattern.Hits.Num(),*UEnum::GetValueAsString(Result));
	if (TryFinishBattle()) { return true; }
	if (!bLastHit)
	{
		if (PlayerCombatant->IsDefeated()) { bWaitingForEnemyAnimation=false; return FinishEnemyAction(); }
	}
	if (!bLastHit)
	{
		++EnemyHitIndex;
		if (!TryTransitionTo(EBattleState::EnemyAction)) { ReturnToExploration(); return false; }
		ScheduleEnemyHit();
		return true;
	}
	return bWaitingForEnemyAnimation || bParryCounterActive ? true : FinishEnemyAction();
}

bool UBattleCoordinator::NotifyParryCounterHit(const int64 ActionId)
{
	if (!bParryCounterActive || !ActionId || ActionId != ActiveEnemyActionId || bParryCounterHitApplied
		|| CurrentState != EBattleState::DefenseWindow || bActionInProgress || bTransitionInProgress) { return false; }
	if (!HasValidActiveBattle()) { ReturnToExploration(); return false; }
	TGuardValue<bool> Guard(bActionInProgress, true);
	bParryCounterHitApplied = true;
	// Stop the enemy's recovery before the counter's additive hit reaction is played.
	if (EnemyPresentation.IsValid() && !EnemyPresentation->IsHeldForCounter()) { EnemyPresentation->StopPresentation(); bWaitingForEnemyAnimation=false; }
	if (!ApplyCombatEffect(EnemyCombatant.Get(), PendingCounterDamage, ECombatNumberKind::Damage)) { ReturnToExploration(); return false; }
	// Death cancels the attack montage; it will no longer send an attack-finished callback.
	if (EnemyCombatant->IsDefeated()) { bWaitingForEnemyAnimation=false; }
	UE_LOG(LogBattleCoordinator, Log, TEXT("Parry counter impact: Id=%lld Damage=%.1f"), ActionId, PendingCounterDamage);
	TryFinishBattle();
	return true;
}

bool UBattleCoordinator::NotifyParryCounterFinished(const int64 ActionId, const bool bInterrupted)
{
	if (!bParryCounterActive || !ActionId || ActionId != ActiveEnemyActionId
		|| CurrentState != EBattleState::DefenseWindow || bActionInProgress || bTransitionInProgress) { return false; }
	TGuardValue<bool> Guard(bActionInProgress, true);
	bParryCounterActive = false;
	GetWorld()->GetTimerManager().ClearTimer(ParryCounterTimerHandle);
	StopCounterPresentation();
	CounterPresentation.Reset();
	UE_LOG(LogBattleCoordinator, Log, TEXT("Parry counter finished: Id=%lld Interrupted=%d"), ActionId, bInterrupted);
	if (EnemyPresentation.IsValid() && EnemyPresentation->IsHeldForCounter()) { bWaitingForEnemyAnimation=EnemyPresentation->ReturnAfterCounter(); }
	return bWaitingForEnemyAnimation ? true : FinishEnemyAction();
}

void UBattleCoordinator::HandleParryCounterTimeout(const int64 ActionId)
{
	if (bParryCounterActive && ActionId == ActiveEnemyActionId)
	{
		UE_LOG(LogBattleCoordinator, Warning, TEXT("Parry counter timed out; cancelling remaining counter."));
		NotifyParryCounterFinished(ActionId, true);
	}
}

bool UBattleCoordinator::FinishEnemyAction()
{
	if (CurrentState==EBattleState::Victory || CurrentState==EBattleState::Defeat) { return true; }
	if (bParryCounterActive) { return true; }
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

void UBattleCoordinator::ClearEnemyAction()
{
	if (BattlePresentation.IsValid()) { BattlePresentation->ReleaseParryFeedback(); }
	bParryCounterActive = false;
	bParryCounterHitApplied = false;
	StopCounterPresentation();
	CounterPresentation.Reset();
	ActiveEnemyActionId = 0;
	EnemyHitIndex=0; bAllEnemyHitsParried=true; bEnemyAttackStarted=false;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DefenseWindowTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(EnemyWindowOpenTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(EnemyActionTimeoutTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(ParryCounterTimerHandle);
	}
	if (EnemyPresentation.IsValid()) { EnemyPresentation->StopPresentation(); }
	EnemyPresentation.Reset();
	PendingDefenseResult = EBattleDefenseResult::None;
	bEnemyHitResolved = false;
	bEnemyHitNotifyReceived = false;
	bWaitingForEnemyAnimation = false;
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

void UBattleCoordinator::ClearBattleTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnemyTelegraphTimerHandle);
		World->GetTimerManager().ClearTimer(DefenseWindowTimerHandle);
		World->GetTimerManager().ClearTimer(TurnTransitionTimer);
	}
	EnemyTelegraphTimerHandle.Invalidate();
	DefenseWindowTimerHandle.Invalidate();
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
	PendingAction=nullptr;PendingItem=nullptr;ActiveItem=nullptr;ActiveRules=nullptr;ItemStocks.Reset();EntryItemStocks.Reset();ActionUses.Reset(); EntryActionUses.Reset();
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
	ActiveCounterAction=nullptr;
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
				if (bActionInProgress || bTransitionInProgress) { return true; }
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
	Timers.ClearTimer(IntroTimer); Timers.ClearTimer(TargetConfirmationTimer); Timers.ClearTimer(PlayerActionTimerHandle);
	Timers.ClearTimer(EnemyWindowOpenTimerHandle); Timers.ClearTimer(EnemyActionTimeoutTimerHandle); Timers.ClearTimer(ParryCounterTimerHandle);
	GetWorld()->GetSubsystem<UActionQTESubsystem>()->CancelQTE(QTESessionId);
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
	if (!bResultReady || bActionInProgress || bTransitionInProgress || CurrentState!=EBattleState::Victory) { return false; }
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
	if (!bResultReady || bActionInProgress || bTransitionInProgress || CurrentState!=EBattleState::Defeat
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
	if (bTargetNotification || bSelectingTarget || bActionInProgress || bTransitionInProgress || !CanRequestPlayerCommand(Command)) { return false; }
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
	if (!bSelectingTarget || bTargetNotification || bActionInProgress || bTransitionInProgress) { return false; }
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
	if (!bSelectingTarget || bTargetNotification || bActionInProgress || bTransitionInProgress) { return false; }
	bSelectingTarget = false;
	PendingAction=nullptr;PendingItem=nullptr;
	Targets.Reset();
	BroadcastTargetSelection();
	return true;
}

bool UBattleCoordinator::ConfirmTargetSelection()
{
	if (!bSelectingTarget || bTargetNotification || bActionInProgress || bTransitionInProgress) { return false; }
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
 if (bSelectingTarget || bConfirmingTarget || bTargetNotification || bTransitionInProgress || bActionInProgress || !CanRequestAction(Action)) { return false; }
 PendingAction=Action;
 return ExecutePlayerCommand(EBattlePlayerCommand::CharacterAction);
}
bool UBattleCoordinator::BeginActionSelection(UCharacterActionData* Action)
{
 if (bSelectingTarget || bConfirmingTarget || bTargetNotification || bActionInProgress || bTransitionInProgress || !CanRequestAction(Action)) { return false; }
 PendingAction=Action;
 if (BeginCommandSelection(EBattlePlayerCommand::CharacterAction)) { return true; }
 PendingAction=nullptr; return false;
}
void UBattleCoordinator::StopCounterPresentation()
{
 if (!CounterPresentation.IsValid()) { return; }
 if (ActiveCounterAction && CounterPresentation->GetClass()->ImplementsInterface(UCharacterActionExecutor::StaticClass()))
 { ICharacterActionExecutor::Execute_StopCharacterAction(CounterPresentation.Get()); }
 else if (CounterPresentation->GetClass()->ImplementsInterface(UParryCounterProvider::StaticClass()))
 { IParryCounterProvider::Execute_StopCounterAction(CounterPresentation.Get()); }
 ActiveCounterAction=nullptr;
}
bool UBattleCoordinator::BeginCharacterCounter()
{
 if (!bParryCounterActive || !ActiveCounterAction || !CounterPresentation.IsValid()) { return false; }
	if (BattlePresentation.IsValid()) { BattlePresentation->PresentCounterAction(ActiveCounterAction); }
 if (auto* Defense=ActivePlayerPawn->FindComponentByClass<UDefensePresentationComponent>()) { Defense->StopPresentation(); }
 const float Duration=ICharacterActionExecutor::Execute_GetCharacterActionDuration(CounterPresentation.Get(),ActiveCounterAction,true);
 if (ICharacterActionExecutor::Execute_StartCharacterAction(CounterPresentation.Get(),ActiveCounterAction,ActiveEnemyActionId,ActiveEnemyActor.Get(),true))
 {
  GetWorld()->GetTimerManager().SetTimer(ParryCounterTimerHandle,FTimerDelegate::CreateUObject(this,&ThisClass::HandleParryCounterTimeout,ActiveEnemyActionId),Duration+1,false);
  return true;
 }
 bParryCounterActive=false; StopCounterPresentation(); CounterPresentation.Reset(); return false;
}
void UBattleCoordinator::NotifyCounterFeedbackFinished(int64 ActionId)
{
 if (!bParryCounterActive || !ActionId || ActionId!=ActiveEnemyActionId || CurrentState!=EBattleState::DefenseWindow) { return; }
 if (!HasValidActiveBattle() || !BeginCharacterCounter()) { ReturnToExploration(); }
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
 if (bSelectingTarget || bConfirmingTarget || bTargetNotification || bActionInProgress || bTransitionInProgress || !CanUseItem(Item)) { return false; }
 PendingItem=Item;
 if (BeginCommandSelection(EBattlePlayerCommand::UseItem)) { return true; }
 PendingItem=nullptr;return false;
}
