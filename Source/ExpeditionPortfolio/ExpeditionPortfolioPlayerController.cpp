// Copyright Epic Games, Inc. All Rights Reserved.


#include "ExpeditionPortfolioPlayerController.h"
#include "Combat/BattleCoordinator.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "ExpeditionPortfolio.h"
#include "Widgets/Input/SVirtualJoystick.h"

void AExpeditionPortfolioPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalPlayerController())
	{
		if (UBattleCoordinator* Coordinator = GetWorld()->GetSubsystem<UBattleCoordinator>())
		{
			BattleStateChangedHandle = Coordinator->OnBattleStateChanged().AddUObject(this, &ThisClass::HandleBattleStateChanged);
			SetExplorationInputEnabled(Coordinator->GetCurrentState() == EBattleState::Exploration);
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

	bExplorationInputEnabled = false;
	SetExplorationInputEnabled(true);

	if (UBattleCoordinator* Coordinator = GetWorld()->GetSubsystem<UBattleCoordinator>())
	{
		if (Coordinator->GetCurrentState() != EBattleState::Exploration)
		{
			SetExplorationInputEnabled(false);
		}
	}
}

void AExpeditionPortfolioPlayerController::HandleBattleStateChanged(
	const EBattleState PreviousState,
	const EBattleState NewState)
{
	if (NewState == EBattleState::Exploration)
	{
		SetExplorationInputEnabled(true);
		return;
	}

	if (PreviousState == EBattleState::Exploration)
	{
		SetExplorationInputEnabled(false);
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

bool AExpeditionPortfolioPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
