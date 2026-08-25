// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CombatHUDWidget.h"

#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"
#include "Engine/World.h"

void UCombatHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BattleCoordinator = GetWorld() != nullptr
		? GetWorld()->GetSubsystem<UBattleCoordinator>()
		: nullptr;

	if (BattleCoordinator.IsValid())
	{
		BattleStateChangedHandle = BattleCoordinator->OnBattleStateChanged().AddUObject(
			this,
			&ThisClass::HandleBattleStateChanged);
		BindActiveCombatants();
	}

	RefreshBattleView();
}

void UCombatHUDWidget::NativeDestruct()
{
	UnbindActiveCombatants();

	if (BattleCoordinator.IsValid() && BattleStateChangedHandle.IsValid())
	{
		BattleCoordinator->OnBattleStateChanged().Remove(BattleStateChangedHandle);
	}
	BattleStateChangedHandle.Reset();
	BattleCoordinator.Reset();

	Super::NativeDestruct();
}

bool UCombatHUDWidget::RequestPlayerCommand(const EBattlePlayerCommand Command)
{
	return BattleCoordinator.IsValid() && BattleCoordinator->RequestPlayerCommand(Command);
}

bool UCombatHUDWidget::RequestBasicAttack()
{
	return RequestPlayerCommand(EBattlePlayerCommand::BasicAttack);
}

bool UCombatHUDWidget::RequestPowerStrike()
{
	return RequestPlayerCommand(EBattlePlayerCommand::PowerStrike);
}

void UCombatHUDWidget::HandleBattleStateChanged(
	const EBattleState PreviousState,
	const EBattleState NewState)
{
	(void)PreviousState;
	(void)NewState;
	BindActiveCombatants();
	RefreshBattleView();
}

void UCombatHUDWidget::HandleCombatantHealthChanged(const float PreviousHealth, const float NewHealth)
{
	(void)PreviousHealth;
	(void)NewHealth;
	RefreshBattleView();
}

void UCombatHUDWidget::HandleCombatantDefeated()
{
	RefreshBattleView();
}

void UCombatHUDWidget::BindActiveCombatants()
{
	UCombatantComponent* NewPlayerCombatant = BattleCoordinator.IsValid()
		? BattleCoordinator->GetPlayerCombatant()
		: nullptr;
	UCombatantComponent* NewEnemyCombatant = BattleCoordinator.IsValid()
		? BattleCoordinator->GetEnemyCombatant()
		: nullptr;

	if (PlayerCombatant.Get() == NewPlayerCombatant && EnemyCombatant.Get() == NewEnemyCombatant)
	{
		return;
	}

	UnbindActiveCombatants();
	PlayerCombatant = NewPlayerCombatant;
	EnemyCombatant = NewEnemyCombatant;

	if (PlayerCombatant.IsValid())
	{
		PlayerHealthChangedHandle = PlayerCombatant->OnHealthChanged().AddUObject(
			this,
			&ThisClass::HandleCombatantHealthChanged);
		PlayerDefeatedHandle = PlayerCombatant->OnDefeated().AddUObject(
			this,
			&ThisClass::HandleCombatantDefeated);
	}

	if (EnemyCombatant.IsValid())
	{
		EnemyHealthChangedHandle = EnemyCombatant->OnHealthChanged().AddUObject(
			this,
			&ThisClass::HandleCombatantHealthChanged);
		EnemyDefeatedHandle = EnemyCombatant->OnDefeated().AddUObject(
			this,
			&ThisClass::HandleCombatantDefeated);
	}
}

void UCombatHUDWidget::UnbindActiveCombatants()
{
	if (PlayerCombatant.IsValid())
	{
		PlayerCombatant->OnHealthChanged().Remove(PlayerHealthChangedHandle);
		PlayerCombatant->OnDefeated().Remove(PlayerDefeatedHandle);
	}
	if (EnemyCombatant.IsValid())
	{
		EnemyCombatant->OnHealthChanged().Remove(EnemyHealthChangedHandle);
		EnemyCombatant->OnDefeated().Remove(EnemyDefeatedHandle);
	}

	PlayerHealthChangedHandle.Reset();
	PlayerDefeatedHandle.Reset();
	EnemyHealthChangedHandle.Reset();
	EnemyDefeatedHandle.Reset();
	PlayerCombatant.Reset();
	EnemyCombatant.Reset();
}

void UCombatHUDWidget::RefreshBattleView()
{
	const EBattleState BattleState = BattleCoordinator.IsValid()
		? BattleCoordinator->GetCurrentState()
		: EBattleState::Exploration;
	SetVisibility(BattleState == EBattleState::Exploration
		? ESlateVisibility::Collapsed
		: ESlateVisibility::Visible);

	const float PlayerCurrentHealth = PlayerCombatant.IsValid() ? PlayerCombatant->GetCurrentHealth() : 0.0f;
	const float PlayerMaxHealth = PlayerCombatant.IsValid() ? PlayerCombatant->GetMaxHealth() : 0.0f;
	const float EnemyCurrentHealth = EnemyCombatant.IsValid() ? EnemyCombatant->GetCurrentHealth() : 0.0f;
	const float EnemyMaxHealth = EnemyCombatant.IsValid() ? EnemyCombatant->GetMaxHealth() : 0.0f;
	const bool bCanUseBasicAttack = BattleCoordinator.IsValid()
		&& BattleCoordinator->CanRequestPlayerCommand(EBattlePlayerCommand::BasicAttack);
	const bool bCanUsePowerStrike = BattleCoordinator.IsValid()
		&& BattleCoordinator->CanRequestPlayerCommand(EBattlePlayerCommand::PowerStrike);

	BP_OnBattleViewUpdated(
		BattleState,
		PlayerCurrentHealth,
		PlayerMaxHealth,
		EnemyCurrentHealth,
		EnemyMaxHealth,
		bCanUseBasicAttack,
		bCanUsePowerStrike);
}
