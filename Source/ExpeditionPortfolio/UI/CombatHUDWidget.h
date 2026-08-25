// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Combat/BattlePlayerCommand.h"
#include "Combat/BattleState.h"
#include "CoreMinimal.h"
#include "CombatHUDWidget.generated.h"

class UBattleCoordinator;
class UCombatantComponent;

UCLASS(Abstract, Blueprintable)
class EXPEDITIONPORTFOLIO_API UCombatHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Combat|Commands")
	bool RequestPlayerCommand(EBattlePlayerCommand Command);

	UFUNCTION(BlueprintCallable, Category = "Combat|Commands")
	bool RequestBasicAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat|Commands")
	bool RequestPowerStrike();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Presentation", meta = (DisplayName = "Battle View Updated"))
	void BP_OnBattleViewUpdated(
		EBattleState BattleState,
		float PlayerCurrentHealth,
		float PlayerMaxHealth,
		float EnemyCurrentHealth,
		float EnemyMaxHealth,
		bool bCanUseBasicAttack,
		bool bCanUsePowerStrike);

private:
	void HandleBattleStateChanged(EBattleState PreviousState, EBattleState NewState);
	void HandleCombatantHealthChanged(float PreviousHealth, float NewHealth);
	void HandleCombatantDefeated();
	void BindActiveCombatants();
	void UnbindActiveCombatants();
	void RefreshBattleView();

	TWeakObjectPtr<UBattleCoordinator> BattleCoordinator;
	TWeakObjectPtr<UCombatantComponent> PlayerCombatant;
	TWeakObjectPtr<UCombatantComponent> EnemyCombatant;
	FDelegateHandle BattleStateChangedHandle;
	FDelegateHandle PlayerHealthChangedHandle;
	FDelegateHandle PlayerDefeatedHandle;
	FDelegateHandle EnemyHealthChangedHandle;
	FDelegateHandle EnemyDefeatedHandle;
};
