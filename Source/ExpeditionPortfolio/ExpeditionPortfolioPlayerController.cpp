// Copyright Epic Games, Inc. All Rights Reserved.


#include "ExpeditionPortfolioPlayerController.h"
#include "Combat/BattleCoordinator.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Blueprint/UserWidget.h"
#include "ExpeditionPortfolio.h"
#include "UI/CombatHUDWidget.h"
#include "Widgets/Input/SVirtualJoystick.h"

void AExpeditionPortfolioPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalPlayerController())
	{
		if (UBattleCoordinator* Coordinator = GetWorld()->GetSubsystem<UBattleCoordinator>())
		{
			BattleStateChangedHandle = Coordinator->OnBattleStateChanged().AddUObject(this, &ThisClass::HandleBattleStateChanged);
			const EBattleState CurrentState = Coordinator->GetCurrentState();
			SetExplorationInputEnabled(CurrentState == EBattleState::Exploration);
			SetPlayerCommandInputEnabled(CurrentState == EBattleState::PlayerCommand);
			SetDefenseInputEnabled(CurrentState == EBattleState::DefenseWindow);
			SetBattleInteractionEnabled(CurrentState != EBattleState::Exploration);
		}

		if (CombatHUDWidgetClass != nullptr)
		{
			CombatHUDWidget = CreateWidget<UCombatHUDWidget>(this, CombatHUDWidgetClass);
			if (CombatHUDWidget != nullptr)
			{
				CombatHUDWidget->AddToPlayerScreen(10);
			}
		}
	}


	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

			MobileControlsWidget->SetIsEnabled(bExplorationInputEnabled);
		} else {

			UE_LOG(LogExpeditionPortfolio, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AExpeditionPortfolioPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CombatHUDWidget != nullptr)
	{
		CombatHUDWidget->RemoveFromParent();
		CombatHUDWidget = nullptr;
	}

	if (BattleStateChangedHandle.IsValid())
	{
		if (UBattleCoordinator* Coordinator = GetWorld()->GetSubsystem<UBattleCoordinator>())
		{
			Coordinator->OnBattleStateChanged().Remove(BattleStateChangedHandle);
		}
		BattleStateChangedHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void AExpeditionPortfolioPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (BasicAttackInputAction != nullptr)
		{
			EnhancedInputComponent->BindAction(
				BasicAttackInputAction,
				ETriggerEvent::Started,
				this,
				&ThisClass::HandleBasicAttackInput);
		}
		if (PowerStrikeInputAction != nullptr)
		{
			EnhancedInputComponent->BindAction(
				PowerStrikeInputAction,
				ETriggerEvent::Started,
				this,
				&ThisClass::HandlePowerStrikeInput);
		}
		if (ParryInputAction != nullptr)
		{
			EnhancedInputComponent->BindAction(
				ParryInputAction,
				ETriggerEvent::Started,
				this,
				&ThisClass::HandleParryInput);
		}
		if (DodgeInputAction != nullptr)
		{
			EnhancedInputComponent->BindAction(
				DodgeInputAction,
				ETriggerEvent::Started,
				this,
				&ThisClass::HandleDodgeInput);
		}
	}

	bExplorationInputEnabled = false;
	SetExplorationInputEnabled(true);

	if (UBattleCoordinator* Coordinator = GetWorld()->GetSubsystem<UBattleCoordinator>())
	{
		if (Coordinator->GetCurrentState() != EBattleState::Exploration)
		{
			SetExplorationInputEnabled(false);
		}
		SetPlayerCommandInputEnabled(Coordinator->GetCurrentState() == EBattleState::PlayerCommand);
		SetDefenseInputEnabled(Coordinator->GetCurrentState() == EBattleState::DefenseWindow);
	}
}

void AExpeditionPortfolioPlayerController::HandleBattleStateChanged(
	const EBattleState PreviousState,
	const EBattleState NewState)
{
	(void)PreviousState;
	SetExplorationInputEnabled(NewState == EBattleState::Exploration);
	SetPlayerCommandInputEnabled(NewState == EBattleState::PlayerCommand);
	SetDefenseInputEnabled(NewState == EBattleState::DefenseWindow);
	SetBattleInteractionEnabled(NewState != EBattleState::Exploration);
}

void AExpeditionPortfolioPlayerController::HandleBasicAttackInput()
{
	if (UBattleCoordinator* Coordinator = GetWorld()->GetSubsystem<UBattleCoordinator>())
	{
		Coordinator->RequestPlayerCommand(EBattlePlayerCommand::BasicAttack);
	}
}

void AExpeditionPortfolioPlayerController::HandlePowerStrikeInput()
{
	if (UBattleCoordinator* Coordinator = GetWorld()->GetSubsystem<UBattleCoordinator>())
	{
		Coordinator->RequestPlayerCommand(EBattlePlayerCommand::PowerStrike);
	}
}

void AExpeditionPortfolioPlayerController::HandleParryInput()
{
	if (UBattleCoordinator* Coordinator = GetWorld()->GetSubsystem<UBattleCoordinator>())
	{
		Coordinator->RequestDefenseCommand(EBattleDefenseCommand::Parry);
	}
}

void AExpeditionPortfolioPlayerController::HandleDodgeInput()
{
	if (UBattleCoordinator* Coordinator = GetWorld()->GetSubsystem<UBattleCoordinator>())
	{
		Coordinator->RequestDefenseCommand(EBattleDefenseCommand::Dodge);
	}
}

void AExpeditionPortfolioPlayerController::SetExplorationInputEnabled(const bool bEnabled)
{
	if (!IsLocalPlayerController() || bExplorationInputEnabled == bEnabled)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem == nullptr)
	{
		return;
	}

	const auto UpdateMappingContext = [Subsystem, bEnabled](UInputMappingContext* Context)
	{
		if (!IsValid(Context))
		{
			return;
		}

		if (bEnabled)
		{
			Subsystem->AddMappingContext(Context, 0);
		}
		else
		{
			Subsystem->RemoveMappingContext(Context);
		}
	};

	for (UInputMappingContext* Context : DefaultMappingContexts)
	{
		UpdateMappingContext(Context);
	}

	if (!ShouldUseTouchControls())
	{
		for (UInputMappingContext* Context : MobileExcludedMappingContexts)
		{
			UpdateMappingContext(Context);
		}
	}

	SetIgnoreMoveInput(!bEnabled);
	SetIgnoreLookInput(!bEnabled);
	if (MobileControlsWidget != nullptr)
	{
		MobileControlsWidget->SetIsEnabled(bEnabled);
	}
	bExplorationInputEnabled = bEnabled;
}

void AExpeditionPortfolioPlayerController::SetPlayerCommandInputEnabled(const bool bEnabled)
{
	if (!IsLocalPlayerController() || bPlayerCommandInputEnabled == bEnabled)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem == nullptr)
	{
		return;
	}

	if (PlayerCommandMappingContext != nullptr)
	{
		if (bEnabled)
		{
			Subsystem->AddMappingContext(PlayerCommandMappingContext, 1);
		}
		else
		{
			Subsystem->RemoveMappingContext(PlayerCommandMappingContext);
		}
	}
	bPlayerCommandInputEnabled = bEnabled;
}

void AExpeditionPortfolioPlayerController::SetDefenseInputEnabled(const bool bEnabled)
{
	if (!IsLocalPlayerController() || bDefenseInputEnabled == bEnabled)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem == nullptr)
	{
		return;
	}

	if (DefenseMappingContext != nullptr)
	{
		if (bEnabled)
		{
			Subsystem->AddMappingContext(DefenseMappingContext, 1);
		}
		else
		{
			Subsystem->RemoveMappingContext(DefenseMappingContext);
		}
	}
	bDefenseInputEnabled = bEnabled;
}

void AExpeditionPortfolioPlayerController::SetBattleInteractionEnabled(const bool bEnabled)
{
	if (!IsLocalPlayerController() || bBattleInteractionEnabled == bEnabled)
	{
		return;
	}

	bShowMouseCursor = bEnabled;
	if (bEnabled)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
	bBattleInteractionEnabled = bEnabled;
}

bool AExpeditionPortfolioPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
