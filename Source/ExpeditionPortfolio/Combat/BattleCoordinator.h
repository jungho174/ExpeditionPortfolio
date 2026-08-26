// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/BattleDefense.h"
#include "Combat/BattlePlayerCommand.h"
#include "Combat/BattleState.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "BattleCoordinator.generated.h"

class AActor;
class APawn;
class UCombatantComponent;

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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnDefenseResultChangedBlueprint,
	EBattleDefenseResult, Result);

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UBattleCoordinator : public UWorldSubsystem
{
	GENERATED_BODY()

public:
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

	bool RequestBattleStart(AActor* EncounterSource, APawn* PlayerPawn, AActor* EnemyActor);

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

	UPROPERTY(BlueprintAssignable, Category = "Combat|Battle")
	FOnBattleStateChangedBlueprint BattleStateChangedBlueprint;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Battle")
	FOnDefenseResultChangedBlueprint DefenseResultChangedBlueprint;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FBattleCoordinatorValidTransitionsTest;
	friend class FBattleCoordinatorInvalidTransitionsTest;
	friend class FBattleCoordinatorDelegateNotificationsTest;
#endif

	enum class ECompletedAction : uint8
	{
		Player,
		Enemy
	};

	bool TryTransitionTo(EBattleState NewState);
	bool IsTransitionAllowed(EBattleState NewState) const;

	bool HasValidActiveBattle() const;
	bool ExecutePlayerCommand(EBattlePlayerCommand Command);
	bool StartEnemyTelegraph();
	void HandleEnemyTelegraphFinished();
	void HandleDefenseWindowExpired();
	bool ResolveDefenseOutcome(EBattleDefenseResult Result);
	void SetDefenseResult(EBattleDefenseResult Result);
	void ClearBattleTimers();
	bool ResolveTurnEnd(ECompletedAction CompletedAction);
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
	FTimerHandle EnemyTelegraphTimerHandle;
	FTimerHandle DefenseWindowTimerHandle;
	EBattleDefenseResult LastDefenseResult = EBattleDefenseResult::None;
	bool bTransitionInProgress = false;
	bool bActionInProgress = false;
	bool bPowerStrikeUsed = false;

	static constexpr float PlayerBasicAttackDamage = 25.0f;
	static constexpr float PlayerPowerStrikeDamage = 40.0f;
	static constexpr float EnemyBasicAttackDamage = 20.0f;
	static constexpr float ParryCounterDamage = 15.0f;
	static constexpr float EnemyTelegraphDuration = 1.0f;
	static constexpr float DefenseWindowDuration = 0.6f;
	static constexpr float ParrySuccessDuration = 0.2f;
};
