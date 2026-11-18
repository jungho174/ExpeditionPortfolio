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
#include "UI/CombatNumberWidget.h"
#include "UI/AttackTimingWidget.h"
#include "UI/BattlePresentationComponent.h"
#include "UI/QTEPresentationComponent.h"
#include "UI/BattleResultPresentationComponent.h"
#include "Combat/ActionQTESubsystem.h"
#include "Widgets/Input/SVirtualJoystick.h"

AExpeditionPortfolioPlayerController::AExpeditionPortfolioPlayerController()
{
	BattlePresentation = CreateDefaultSubobject<UBattlePresentationComponent>(TEXT("BattlePresentation"));
	QTEPresentation = CreateDefaultSubobject<UQTEPresentationComponent>(TEXT("QTEPresentation"));
	BattleResultPresentation = CreateDefaultSubobject<UBattleResultPresentationComponent>(TEXT("BattleResultPresentation"));
}

void AExpeditionPortfolioPlayerController::HandleAttackTimingInput()
{
	if (auto* Battle=GetWorld()->GetSubsystem<UBattleCoordinator>())
	{
		if (TargetConfirmKey==AttackTimingKey && Battle->IsSelectingTarget()) { Battle->ConfirmTargetSelection(); return; }
		if (Battle->IsConfirmingTarget()) { return; }
		if (AttackTimingKey==EKeys::SpaceBar && Battle->GetCurrentState()==EBattleState::PlayerCommand)
		{
			HandleMenuConfirmInput(); return;
		}
	}
	if (auto* QTE = GetWorld()->GetSubsystem<UActionQTESubsystem>())
	{
		const int64 Session = QTE->GetView().SessionId;
		auto* Battle = GetWorld()->GetSubsystem<UBattleCoordinator>();
		if (Session && Battle && Battle->GetAttackTimingView().SessionId == Session) { Battle->RequestAttackTimingInput(); }
		else { QTE->SubmitInput(Session); }
	}
}

void AExpeditionPortfolioPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalPlayerController())
	{
		if (UBattleCoordinator* Coordinator = GetWorld()->GetSubsystem<UBattleCoordinator>())
		{
			BattleStateChangedHandle = Coordinator->OnBattleStateChanged().AddUObject(this, &ThisClass::HandleBattleStateChanged);
			TargetSelectionHandle = Coordinator->OnTargetSelectionChanged().AddUObject(this, &ThisClass::HandleTargetSelectionChanged);
			const EBattleState CurrentState = Coordinator->GetCurrentState();
			SetExplorationInputEnabled(CurrentState == EBattleState::Exploration);
			SetPlayerCommandInputEnabled(CurrentState == EBattleState::PlayerCommand);
			SetDefenseInputEnabled(CurrentState == EBattleState::DefenseWindow);
			SetBattleInteractionEnabled(CurrentState == EBattleState::PlayerCommand);
		}

		if (CombatHUDWidgetClass != nullptr)
		{
			CombatHUDWidget = CreateWidget<UCombatHUDWidget>(this, CombatHUDWidgetClass);
			if (CombatHUDWidget != nullptr)
			{
				CombatHUDWidget->AddToPlayerScreen(10);
			}
		}
		AttackTimingWidget = CreateWidget<UAttackTimingWidget>(this, AttackTimingWidgetClass ? AttackTimingWidgetClass.Get() : UAttackTimingWidget::StaticClass());
		CombatNumberWidget=CreateWidget<UCombatNumberWidget>(this,UCombatNumberWidget::StaticClass());
		if (CombatNumberWidget)
		{
			CombatNumberWidget->PresentationData=CombatNumberData;
			CombatNumberWidget->AddToPlayerScreen(30);
		}
		if (AttackTimingWidget)
		{
			AttackTimingWidget->InputLabel = AttackTimingKey == EKeys::SpaceBar ? FText::FromString(TEXT("SPACE")) : AttackTimingKey.GetDisplayName();
			AttackTimingWidget->AddToPlayerScreen(20);
			AttackTimingWidget->SetDesiredSizeInViewport(FVector2D(260,260));
			AttackTimingWidget->SetAnchorsInViewport(FAnchors(0.5f,0.7f));
			AttackTimingWidget->SetAlignmentInViewport(FVector2D(0.5f,0.5f));
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
	if (CombatNumberWidget) { CombatNumberWidget->RemoveFromParent(); CombatNumberWidget=nullptr; }
	if (auto* Battle = GetWorld()->GetSubsystem<UBattleCoordinator>()) { Battle->OnTargetSelectionChanged().Remove(TargetSelectionHandle); }
	if (AttackTimingWidget) { AttackTimingWidget->RemoveFromParent(); AttackTimingWidget = nullptr; }
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
	InputComponent->BindKey(AttackTimingKey, IE_Pressed, this, &ThisClass::HandleAttackTimingInput);
	// Do not consume exploration W/S on the controller's higher-priority input component.
	InputComponent->BindKey(EKeys::W,IE_Pressed,this,&ThisClass::HandleMenuPreviousInput).bConsumeInput=false;
	InputComponent->BindKey(EKeys::S,IE_Pressed,this,&ThisClass::HandleMenuNextInput).bConsumeInput=false;
	if (AttackTimingKey!=EKeys::SpaceBar && TargetConfirmKey!=EKeys::SpaceBar)
	{
		InputComponent->BindKey(EKeys::SpaceBar,IE_Pressed,this,&ThisClass::HandleMenuConfirmInput).bConsumeInput=false;
	}
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ThisClass::HandleThirdMenuInput);
	InputComponent->BindKey(EKeys::BackSpace, IE_Pressed, this, &ThisClass::HandleMenuBackInput);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::HandleMenuBackInput);
	if (TargetConfirmKey!=AttackTimingKey) { InputComponent->BindKey(TargetConfirmKey, IE_Pressed, this, &ThisClass::HandleTargetConfirm); }
	InputComponent->BindKey(TargetPreviousKey, IE_Pressed, this, &ThisClass::HandleTargetPrevious);
	InputComponent->BindKey(TargetNextKey, IE_Pressed, this, &ThisClass::HandleTargetNext);
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
	SetBattleInteractionEnabled(NewState == EBattleState::PlayerCommand);
	const bool bResult=NewState==EBattleState::Victory || NewState==EBattleState::Defeat;
	if (bResult && AttackTimingWidget) { AttackTimingWidget->SetVisibility(ESlateVisibility::Collapsed); }
}

void AExpeditionPortfolioPlayerController::HandleBasicAttackInput()
{
	if (CombatHUDWidget) { CombatHUDWidget->SelectMenuEntry(1); }
}

void AExpeditionPortfolioPlayerController::HandlePowerStrikeInput()
{
	if (CombatHUDWidget) { CombatHUDWidget->SelectMenuEntry(2); }
}

void AExpeditionPortfolioPlayerController::HandleThirdMenuInput()
{
	if (CombatHUDWidget) { CombatHUDWidget->SelectMenuEntry(3); }
}

void AExpeditionPortfolioPlayerController::HandleMenuPreviousInput()
{
	if (CombatHUDWidget) { CombatHUDWidget->MoveMenuSelection(-1); }
}
void AExpeditionPortfolioPlayerController::HandleMenuNextInput()
{
	if (CombatHUDWidget) { CombatHUDWidget->MoveMenuSelection(1); }
}
void AExpeditionPortfolioPlayerController::HandleMenuConfirmInput()
{
	if (CombatHUDWidget) { CombatHUDWidget->ConfirmMenuSelection(); }
}

void AExpeditionPortfolioPlayerController::HandleMenuBackInput()
{
	if (CombatHUDWidget) { CombatHUDWidget->BackToMainMenu(); }
}

FText AExpeditionPortfolioPlayerController::GetTargetInputHint() const
{
	return FText::Format(FText::FromString(TEXT("{0} / {1}  대상 변경     {2}  확정     Backspace  취소")),
		TargetPreviousKey.GetDisplayName(), TargetNextKey.GetDisplayName(), TargetConfirmKey.GetDisplayName());
}
void AExpeditionPortfolioPlayerController::HandleTargetSelectionChanged()
{
	if (auto* Battle = GetWorld()->GetSubsystem<UBattleCoordinator>())
	{
		SetBattleInteractionEnabled(Battle->GetCurrentState() == EBattleState::PlayerCommand && !Battle->IsSelectingTarget() && !Battle->IsConfirmingTarget());
	}
}
void AExpeditionPortfolioPlayerController::HandleTargetConfirm()
{
	if (auto* Battle = GetWorld()->GetSubsystem<UBattleCoordinator>())
	{
		if (TargetConfirmKey==EKeys::SpaceBar && !Battle->IsSelectingTarget()) { HandleMenuConfirmInput(); return; }
		Battle->ConfirmTargetSelection();
	}
}
void AExpeditionPortfolioPlayerController::HandleTargetPrevious()
{
	if (auto* Battle = GetWorld()->GetSubsystem<UBattleCoordinator>()) { Battle->MoveTargetSelection(-1); }
}
void AExpeditionPortfolioPlayerController::HandleTargetNext()
{
	if (auto* Battle = GetWorld()->GetSubsystem<UBattleCoordinator>()) { Battle->MoveTargetSelection(1); }
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
		FInputModeGameOnly InputMode;
		InputMode.SetConsumeCaptureMouseDown(false);
		SetInputMode(InputMode);
	}
	bBattleInteractionEnabled = bEnabled;
}

bool AExpeditionPortfolioPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
