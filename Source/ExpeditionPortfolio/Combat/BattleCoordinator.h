// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/BattleDefense.h"
#include "Combat/BattlePlayerCommand.h"
#include "Combat/BattleState.h"
#include "Combat/AttackTiming.h"
#include "Combat/BattleFacing.h"
#include "Combat/BattleResult.h"
#include "Combat/CombatNumber.h"
#include "Containers/Ticker.h"
#include "Combat/CombatTargetSelection.h"
#include "Combat/CharacterActionData.h"
#include "Combat/EnemyAttackPatternData.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "BattleCoordinator.generated.h"

class AActor;
class APawn;
class UCombatantComponent;
class UMagicPresentationComponent;
class UEnemyAttackPresentationComponent;
class UPlayerUtilityPresentationComponent;
class UBattlePresentationComponent;
class UActionQTEPatternData;
class UCharacterActionData;
class UCharacterCombatData;
class UCombatItemData;
class UBattleRulesData;
class UItemPresentationComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogBattleCoordinator, Log, All);

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnBattleStateChanged,
	EBattleState /* PreviousState */,
	EBattleState /* NewState */);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnBattleStateChangedBlueprint,
	EBattleState, PreviousState,
	EBattleState, NewState);

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnDefenseResultChanged,
	EBattleDefenseResult /* Result */);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDefenseCommandAccepted, EBattleDefenseCommand, float /* TimeUntilHit */);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerPhaseChanged, EPlayerActionPhase);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatNumber, const FCombatNumber&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatNumberBlueprint, const FCombatNumber&, Number);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnDefenseResultChangedBlueprint,
	EBattleDefenseResult, Result);

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UBattleCoordinator : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable,Category="Combat|Rules") bool ConfigureRules(UBattleRulesData* Rules);
	UFUNCTION(BlueprintPure,Category="Combat|Items") TArray<UCombatItemData*> GetAvailableItems() const;
	UFUNCTION(BlueprintPure,Category="Combat|Items") int32 GetRemainingItems(UCombatItemData* Item) const;
	UFUNCTION(BlueprintPure,Category="Combat|Items") bool CanUseItem(UCombatItemData* Item) const;
	UFUNCTION(BlueprintCallable,Category="Combat|Items") bool BeginItemSelection(UCombatItemData* Item);
	static bool IsItemCommand(EBattlePlayerCommand Command) { return Command==EBattlePlayerCommand::UseItem || Command==EBattlePlayerCommand::UsePotion; }
	UFUNCTION(BlueprintPure,Category="Combat|Character") UCharacterActionData* GetActiveCharacterAction() const { return ActiveAction; }
 UFUNCTION(BlueprintPure,Category="Combat|Character") UCharacterCombatData* GetCharacterActions() const;
 UFUNCTION(BlueprintPure,Category="Combat|Character") bool CanRequestAction(UCharacterActionData* Action) const;
 UFUNCTION(BlueprintPure,Category="Combat|Character") int32 GetRemainingActionUses(UCharacterActionData* Action) const;
 UFUNCTION(BlueprintPure,Category="Combat|Character") bool IsActionExhausted(UCharacterActionData* Action) const;
 UFUNCTION(BlueprintCallable,Category="Combat|Character") bool RequestPlayerAction(UCharacterActionData* Action);
 UFUNCTION(BlueprintCallable,Category="Combat|Character") bool BeginActionSelection(UCharacterActionData* Action);
 void NotifyCounterFeedbackFinished(int64 ActionId);
	FOnCombatNumber& OnCombatNumber() { return CombatNumberApplied; }
	UPROPERTY(BlueprintAssignable, Category="Combat|Feedback") FOnCombatNumberBlueprint CombatNumberAppliedBlueprint;
	UFUNCTION(BlueprintPure, Category="Combat|Result") FBattleResult GetBattleResult() const { return ResultSnapshot; }
	UFUNCTION(BlueprintPure, Category="Combat|Result") bool IsResultReady() const { return bResultReady; }
	UFUNCTION(BlueprintCallable, Category="Combat|Result") bool ContinueAfterVictory();
	UFUNCTION(BlueprintCallable, Category="Combat|Result") bool RetryBattle();
	UFUNCTION(BlueprintCallable, Category="Combat|Result") bool RetreatFromBattle();
	void NotifyResultReady();
	// Interactive commands wait for enemy or ally target confirmation.
	UFUNCTION(BlueprintCallable, Category="Combat|Target") bool RegisterBattleAlly(AActor* Ally);
	UFUNCTION(BlueprintPure, Category="Combat|Target") bool IsSelectingAlly() const { return bSelectingTarget && IsItemCommand(PendingCommand); }
	UFUNCTION(BlueprintPure, Category="Combat|Commands") bool IsCommandExhausted(EBattlePlayerCommand Command) const;
	FSimpleMulticastDelegate& OnTargetNavigation() { return TargetNavigation; }
	FSimpleMulticastDelegate& OnTargetConfirmed() { return TargetConfirmed; }
	UFUNCTION(BlueprintCallable, Category="Combat|Target") bool BeginCommandSelection(EBattlePlayerCommand Command);
	UFUNCTION(BlueprintCallable, Category="Combat|Target") bool MoveTargetSelection(int32 Direction);
	UFUNCTION(BlueprintCallable, Category="Combat|Target") bool ConfirmTargetSelection();
	UFUNCTION(BlueprintCallable, Category="Combat|Target") bool CancelTargetSelection();
	UFUNCTION(BlueprintPure, Category="Combat|Target") bool IsSelectingTarget() const { return bSelectingTarget; }
	UFUNCTION(BlueprintPure, Category="Combat|Target") bool IsConfirmingTarget() const { return bConfirmingTarget; }
	UFUNCTION(BlueprintPure, Category="Combat|Target") AActor* GetSelectedTarget() const { return Targets.Get(); }
	UFUNCTION(BlueprintPure, Category="Combat|Target") bool IsSelectingAllEnemies() const;
	UFUNCTION(BlueprintPure, Category="Combat|Target") TArray<AActor*> GetSelectedTargets() const;
	UFUNCTION(BlueprintPure, Category="Combat|Target") UCharacterActionData* GetTargetSelectionAction() const;
	// Accepted targets stay available after damage for presentation; HP logic skips defeated targets.
	UFUNCTION(BlueprintPure, Category="Combat|Target") TArray<AActor*> GetActiveActionTargets() const;
	UFUNCTION(BlueprintPure, Category="Combat|Target") bool IsActiveActionAreaAttack() const { return ActiveTargetScope==ECharacterActionTargetScope::AllEnemies; }
	UFUNCTION(BlueprintPure, Category="Combat|Target") int32 GetTargetCount() const { return Targets.Candidates.Num(); }
	FSimpleMulticastDelegate& OnTargetSelectionChanged() { return TargetSelectionChanged; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Combat|Battle")
	EBattleState GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintCallable, Category = "Combat|Battle")
	bool RequestPlayerCommand(EBattlePlayerCommand Command);

	UFUNCTION(BlueprintCallable, Category = "Combat|Battle")
	bool RequestPlayerBasicAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat|Battle")
	bool RequestPlayerPowerStrike();

	UFUNCTION(BlueprintPure, Category = "Combat|Battle")
	bool CanRequestPlayerCommand(EBattlePlayerCommand Command) const;

	UFUNCTION(BlueprintPure, Category="Combat|Items")
	int32 GetRemainingPotions() const { return RemainingPotions; }
	UFUNCTION(BlueprintPure, Category="Combat|Items")
	float GetPotionHealing() const { return PotionHealing; }
	UFUNCTION(BlueprintPure, Category="Combat|Commands")
	float GetMeleeDamage() const;
	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetMagicDamage(EBattlePlayerCommand Command) const;
	UFUNCTION(BlueprintCallable, Category="Combat|Timing") bool RequestAttackTimingInput();
	UFUNCTION(BlueprintPure, Category="Combat|Timing") FAttackTimingView GetAttackTimingView() const;
	UFUNCTION(BlueprintPure, Category="Combat|Presentation") EPlayerActionPhase GetPlayerActionPhase() const { return PlayerActionPhase; }
	FOnPlayerPhaseChanged& OnPlayerPhaseChanged() { return PlayerPhaseChanged; }
	FSimpleMulticastDelegate& OnAttackTimingChanged() { return AttackTimingChanged; }
	void BeginPlayerPreparation(int64 ActionId, float Duration, float SuccessDuration, float Multiplier, bool bTimingEnabled, const UActionQTEPatternData* Pattern = nullptr);
	void NotifyPlayerPreparationFinished(int64 ActionId);
	void NotifyBattleIntroFinished(int64 BattleId);

	UFUNCTION(BlueprintCallable, Category = "Combat|Battle")
	bool RequestDefenseCommand(EBattleDefenseCommand Command);

	UFUNCTION(BlueprintCallable, Category = "Combat|Battle")
	bool RequestParry();

	UFUNCTION(BlueprintCallable, Category = "Combat|Battle")
	bool RequestDodge();

	UFUNCTION(BlueprintPure, Category = "Combat|Battle")
	bool CanRequestDefenseCommand(EBattleDefenseCommand Command) const;

	UFUNCTION(BlueprintPure, Category = "Combat|Battle")
	EBattleDefenseResult GetLastDefenseResult() const { return LastDefenseResult; }

	UFUNCTION(BlueprintPure, Category = "Combat|Pattern")
	bool HasActiveEnemyPattern() const { return bHasActiveEnemyPattern; }

	// A value copy, never the mutable source asset. Empty ID outside an enemy action.
	UFUNCTION(BlueprintPure, Category = "Combat|Pattern")
	FEnemyAttackPatternSnapshot GetActiveEnemyPattern() const;

	bool RequestBattleStart(AActor* EncounterSource, APawn* PlayerPawn, AActor* EnemyActor);
	UFUNCTION(BlueprintCallable, Category="Combat|Encounter")
	bool RequestGroupBattleStart(AActor* EncounterSource, APawn* ExplorationPlayer, const TArray<AActor*>& Enemies, const TArray<APawn*>& AdditionalAllies);
	UFUNCTION(BlueprintPure, Category="Combat|Party") TArray<AActor*> GetBattleAllies() const;
	UFUNCTION(BlueprintPure, Category="Combat|Party") TArray<AActor*> GetBattleEnemies() const;
	UFUNCTION(BlueprintPure, Category="Combat|Turn") AActor* GetActingCombatant() const;
	UFUNCTION(BlueprintPure, Category="Combat|Turn") int32 GetBattleRound() const { return BattleRound; }

	// Only the matching, active action may report a hit or completion.
	bool NotifyPlayerActionHit(int64 ActionId);
	bool NotifyPlayerActionFinished(int64 ActionId, bool bInterrupted);
	UFUNCTION(BlueprintCallable, Category="Combat|Counter") bool NotifyCounterHit(int64 ActionId) { return NotifyParryCounterHit(ActionId); }
	UFUNCTION(BlueprintCallable, Category="Combat|Counter") bool NotifyCounterFinished(int64 ActionId, bool bInterrupted=false) { return NotifyParryCounterFinished(ActionId,bInterrupted); }
	bool NotifyEnemyActionHit(int64 ActionId, int32 HitIndex=0);
	void NotifyEnemyAttackStarted(int64 ActionId);
	UFUNCTION(BlueprintPure,Category="Combat|Pattern") int32 GetEnemyHitIndex() const { return EnemyHitIndex; }
	UFUNCTION(BlueprintPure,Category="Combat|Pattern") int32 GetEnemyHitCount() const { return ActiveEnemyPattern.Hits.Num(); }
	bool NotifyEnemyActionFinished(int64 ActionId, bool bInterrupted);
	bool NotifyParryCounterHit(int64 ActionId);
	bool NotifyParryCounterFinished(int64 ActionId, bool bInterrupted);
	UFUNCTION(BlueprintPure, Category = "Combat|Defense")
	bool IsParryCounterActive() const { return bParryCounterActive; }

	UFUNCTION(BlueprintPure, Category = "Combat|Timing")
	float GetEnemyHitTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Battle")
	AActor* GetActiveEncounterSource() const { return ActiveEncounterSource.Get(); }

	UFUNCTION(BlueprintPure, Category = "Combat|Battle")
	APawn* GetActivePlayerPawn() const { return ActivePlayerPawn.Get(); }

	UFUNCTION(BlueprintPure, Category = "Combat|Battle")
	AActor* GetActiveEnemyActor() const { return ActiveEnemyActor.Get(); }

	UFUNCTION(BlueprintPure, Category = "Combat|Battle")
	UCombatantComponent* GetPlayerCombatant() const { return PlayerCombatant.Get(); }

	UFUNCTION(BlueprintPure, Category = "Combat|Battle")
	UCombatantComponent* GetEnemyCombatant() const { return EnemyCombatant.Get(); }

	FOnBattleStateChanged& OnBattleStateChanged() { return BattleStateChanged; }
	FOnDefenseResultChanged& OnDefenseResultChanged() { return DefenseResultChanged; }
	FOnDefenseCommandAccepted& OnDefenseCommandAccepted() { return DefenseCommandAccepted; }

	UPROPERTY(BlueprintAssignable, Category = "Combat|Battle")
	FOnBattleStateChangedBlueprint BattleStateChangedBlueprint;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Battle")
	FOnDefenseResultChangedBlueprint DefenseResultChangedBlueprint;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	UPROPERTY(Transient) TObjectPtr<UBattleRulesData> ConfiguredRules;
	UPROPERTY(Transient) TObjectPtr<UBattleRulesData> ActiveRules;
	UPROPERTY(Transient) TObjectPtr<UCombatItemData> PendingItem;
	UPROPERTY(Transient) TObjectPtr<UCombatItemData> ActiveItem;
	TMap<FName,int32> ItemStocks,EntryItemStocks;
	TWeakObjectPtr<UItemPresentationComponent> ItemPresentation;
	UCombatItemData* GetDefaultItem() const;
	bool ValidateCharacterSetup(APawn* Player) const;
	bool IsContentFreeTest() const;
	bool PrepareBattleRules(UCombatantComponent* Player);
 UCharacterActionData* ResolveCommandAction(EBattlePlayerCommand Command) const;
 UActorComponent* FindActionExecutor(UCharacterActionData* Action) const;
 void StopCounterPresentation();
 bool BeginCharacterCounter();
 UPROPERTY(Transient) TObjectPtr<UCharacterActionData> PendingAction;
 UPROPERTY(Transient) TObjectPtr<UCharacterActionData> ActiveAction;
 UPROPERTY(Transient) TObjectPtr<UCharacterActionData> ActiveCounterAction;
 TWeakObjectPtr<UActorComponent> ActionPresentation;
 TMap<TWeakObjectPtr<AActor>,TMap<FName,int32>> ActionUses;
 TMap<TWeakObjectPtr<AActor>,TMap<FName,int32>> EntryActionUses;
	bool ApplyCombatEffect(UCombatantComponent* Target, float Amount, ECombatNumberKind Kind);
	FOnCombatNumber CombatNumberApplied;
	struct FParticipantSnapshot
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<UCombatantComponent> Combatant;
		FTransform Transform;
		float Health = 0;
		bool bAlly = false;
		FDelegateHandle HealthHandle;
		FBattleFacingRestore FacingRestore;
	};
	TArray<FParticipantSnapshot> Participants;
	UPROPERTY(Transient) FBattleResult ResultSnapshot;
	bool bResultReady = false;
	int32 EntryPotions = 0;
	bool bEntryPowerStrikeUsed = false;
	FTSTicker::FDelegateHandle ResultCheckTicker;
	void CaptureParticipant(AActor* Actor, bool bAlly);
	void ClearParticipants();
	bool TryFinishBattle();
	bool RestoreEncounter(bool bRetry);
	FCombatTargetSelection Targets;
	TArray<TWeakObjectPtr<AActor>> ActionTargets;
	ECharacterActionTargetScope ActiveTargetScope=ECharacterActionTargetScope::SingleEnemy;
	bool RefreshAreaPrimaryTarget();
	TArray<TWeakObjectPtr<AActor>> BattleAllies;
	TArray<TWeakObjectPtr<AActor>> BattleEnemies;
	TArray<TWeakObjectPtr<AActor>> TurnOrder;
	TWeakObjectPtr<APawn> ExplorationPlayerPawn;
	int32 TurnIndex = 0;
	int32 BattleRound = 1;
	bool AdvanceCombatantTurn();
	bool SetTurnParticipants(AActor* Actor, bool bAllyTurn);
	bool IsRegisteredEnemy(AActor* Actor) const;
	bool HasLivingSide(bool bAlly) const;
	void HandleParticipantDefeat();
	class APlayerController* GetBattlePlayerController() const;
	TWeakObjectPtr<AActor> ItemTarget;
	bool IsRegisteredAlly(AActor* Ally) const;
	bool CanHealAlly(AActor* Ally) const;
	FSimpleMulticastDelegate TargetNavigation;
	FSimpleMulticastDelegate TargetConfirmed;
	bool bConfirmedAlly = false;
public:
	bool IsConfirmedAlly() const { return bConfirmedAlly; }
private:
	float TurnTransitionDelay = 1;
	FTimerHandle TurnTransitionTimer;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FBattleCoordinatorTestAccess;
	friend class FCharacterActionRulesTest;
	friend class FProductionContentValidationTest;
	friend class FSharedItemRulesTest;
	friend class FGroupedBattleLifecycleTest;
	friend class FEnemyDeathLifecycleTest;
	friend class FAreaActionRulesTest;
	bool bContentFreeTest=false;
	float TestTurnTransitionDelay = -1;
#endif
	bool bSelectingTarget = false;
	bool bConfirmingTarget = false;
	FTimerHandle TargetConfirmationTimer;
	void FinishTargetConfirmation(int64 ExpectedBattleId);
	bool bTargetNotification = false;
	EBattlePlayerCommand PendingCommand = EBattlePlayerCommand::BasicAttack;
	FSimpleMulticastDelegate TargetSelectionChanged;
	void BroadcastTargetSelection();
#if WITH_DEV_AUTOMATION_TESTS
	friend class FBattleCoordinatorValidTransitionsTest;
	friend class FBattleCoordinatorInvalidTransitionsTest;
	friend class FBattleCoordinatorDelegateNotificationsTest;
	friend class FBattleCoordinatorPlayerActionLifecycleTest;
	friend class FMagicProjectileLifecycleTest;
	friend class FAttackTimingIntegrationTest;
	friend class FParryCounterLifecycleTest;
	friend class FBattleCoordinatorEnemyActionLifecycleTest;
	friend class FEnemyComboLifecycleTest;
#endif

	enum class ECompletedAction : uint8
	{
		Player,
		Enemy
	};

	bool TryTransitionTo(EBattleState NewState);
	void SetPlayerActionPhase(EPlayerActionPhase Phase);


	bool IsTransitionAllowed(EBattleState NewState) const;

	bool HasValidActiveBattle() const;
	bool ExecutePlayerCommand(EBattlePlayerCommand Command);
	void HandlePlayerActionTimeout(int64 ActionId);
	void ClearPlayerAction();
	bool StartEnemyTelegraph();
	void SelectEnemyPattern();
	void ClearEnemyPattern();
	void HandleEnemyTelegraphFinished();
	void OpenEnemyDefenseWindow();
	void ScheduleEnemyHit();
	bool StartParryCounter();
	void HandleEnemyActionTimeout(int64 ActionId);
	bool FinishEnemyAction();
	void HandleParryCounterTimeout(int64 ActionId);
	void ClearEnemyAction();
	void HandleDefenseWindowExpired();
	bool ResolveDefenseOutcome(EBattleDefenseResult Result);
	void SetDefenseResult(EBattleDefenseResult Result);
	void ClearBattleTimers();
	bool ResolveTurnEnd(ECompletedAction CompletedAction, bool bAfterDelay=false);
	bool ReturnToExploration();
	void ClearBattleContext();
	void LogRejectedCommand(const TCHAR* CommandName, const TCHAR* Reason) const;
	EBattleState CurrentState = EBattleState::Exploration;
	TWeakObjectPtr<AActor> ActiveEncounterSource;
	TWeakObjectPtr<APawn> ActivePlayerPawn;
	TWeakObjectPtr<AActor> ActiveEnemyActor;
	TWeakObjectPtr<UCombatantComponent> PlayerCombatant;
	TWeakObjectPtr<UCombatantComponent> EnemyCombatant;
	FOnBattleStateChanged BattleStateChanged;
	FOnDefenseResultChanged DefenseResultChanged;
	FOnDefenseCommandAccepted DefenseCommandAccepted;
	FTimerHandle EnemyTelegraphTimerHandle;
	FTimerHandle DefenseWindowTimerHandle;
	EBattleDefenseResult LastDefenseResult = EBattleDefenseResult::None;
	bool bTransitionInProgress = false;
	bool bActionInProgress = false;
	bool bPowerStrikeUsed = false;
	FBattleFacingRestore PlayerFacingRestore;
	FBattleFacingRestore EnemyFacingRestore;
	TWeakObjectPtr<UBattlePresentationComponent> BattlePresentation;
	int64 BattleSerial = 0;
	FTimerHandle IntroTimer;
	int64 QTESessionId = 0;
	FDelegateHandle QTEChangedHandle;
	EPlayerActionPhase PlayerActionPhase = EPlayerActionPhase::None;
	FOnPlayerPhaseChanged PlayerPhaseChanged;
	FSimpleMulticastDelegate AttackTimingChanged;
	UPROPERTY(Transient) FEnemyAttackPatternSnapshot ActiveEnemyPattern;
	bool bHasActiveEnemyPattern = false;
	FRandomStream PatternRandom;
	FTimerHandle PlayerActionTimerHandle;
	TWeakObjectPtr<UMagicPresentationComponent> PlayerPresentation;
	TWeakObjectPtr<UPlayerUtilityPresentationComponent> UtilityPresentation;
	EBattlePlayerCommand ActivePlayerCommand = EBattlePlayerCommand::BasicAttack;
	int32 RemainingPotions = 0;
	float PotionHealing = 30.0f;
	float MeleeDamage = 15.0f;
	int64 NextPlayerActionId = 0;
	int64 ActivePlayerActionId = 0;
	float PendingPlayerDamage = 0.0f;
	bool bPlayerHitApplied = false;
	FTimerHandle EnemyWindowOpenTimerHandle;
	FTimerHandle EnemyActionTimeoutTimerHandle;
	TWeakObjectPtr<UEnemyAttackPresentationComponent> EnemyPresentation;
	int64 NextEnemyActionId = 0;
	int64 ActiveEnemyActionId = 0;
	EBattleDefenseResult PendingDefenseResult = EBattleDefenseResult::None;
	bool bEnemyHitResolved = false;
	bool bEnemyHitNotifyReceived = false;
	bool bWaitingForEnemyAnimation = false;
	bool bParryCounterActive = false;
	bool bParryCounterHitApplied = false;
	float PendingCounterDamage = 15.0f;
	TWeakObjectPtr<UActorComponent> CounterPresentation;
	int32 EnemyHitIndex = 0;
	bool bAllEnemyHitsParried = true;
	bool bEnemyAttackStarted = false;
	float EnemyAttackStartedAt = 0;
	FTimerHandle ParryCounterTimerHandle;

	static constexpr float PlayerBasicAttackDamage = 25.0f;
	static constexpr float PlayerPowerStrikeDamage = 40.0f;
	static constexpr float ParryCounterDamage = 15.0f;
};
