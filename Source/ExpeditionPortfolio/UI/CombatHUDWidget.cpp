// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CombatHUDWidget.h"
#include "Combat/CombatantVisualData.h"

#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"
#include "Combat/CharacterCombatData.h"
#include "Combat/CombatItemData.h"
#include "UI/CharacterSkillButton.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/CanvasPanel.h"
#include "Engine/World.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "ExpeditionPortfolioPlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/BattlePresentationComponent.h"

void UCombatHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	for (const TCHAR* Name : {TEXT("BTN_Items"),TEXT("BTN_Skills"),TEXT("BTN_Melee"),TEXT("BTN_WeakMagic"),TEXT("BTN_StrongMagic"),TEXT("BTN_Potion"),TEXT("BTN_MenuBack")})
	{
		if (const UWidget* Button=GetWidgetFromName(Name); Button && !MenuButtonBaseScales.Contains(Name)) { MenuButtonBaseScales.Add(Name,Button->GetRenderTransform().Scale); }
	}
	if (UWidget* Menu = GetWidgetFromName(TEXT("CN_CommandMenu")))
	{
		MenuBaseAngle = Menu->GetRenderTransform().Angle;
		// Subpixel breathing movement must not jump between rounded screen pixels.
		Menu->SetPixelSnapping(EWidgetPixelSnapping::Disabled);
	}
	ResetCommandMenuMotion();
	if (auto* B = Cast<UButton>(GetWidgetFromName(TEXT("BTN_Items")))) { B->OnClicked.AddUniqueDynamic(this, &ThisClass::ClickItems); }
	if (auto* B = Cast<UButton>(GetWidgetFromName(TEXT("BTN_Skills")))) { B->OnClicked.AddUniqueDynamic(this, &ThisClass::ClickSkills); }
	if (auto* B = Cast<UButton>(GetWidgetFromName(TEXT("BTN_Melee")))) { B->OnClicked.AddUniqueDynamic(this, &ThisClass::ClickMelee); }
	if (auto* B = Cast<UButton>(GetWidgetFromName(TEXT("BTN_WeakMagic")))) { B->OnClicked.AddUniqueDynamic(this, &ThisClass::ClickWeak); }
	if (auto* B = Cast<UButton>(GetWidgetFromName(TEXT("BTN_StrongMagic")))) { B->OnClicked.AddUniqueDynamic(this, &ThisClass::ClickStrong); }
	if (auto* B = Cast<UButton>(GetWidgetFromName(TEXT("BTN_Potion")))) { B->OnClicked.AddUniqueDynamic(this, &ThisClass::ClickPotion); }
	if (auto* B = Cast<UButton>(GetWidgetFromName(TEXT("BTN_MenuBack")))) { B->OnClicked.AddUniqueDynamic(this, &ThisClass::ClickBack); }

	BattleCoordinator = GetWorld() != nullptr
		? GetWorld()->GetSubsystem<UBattleCoordinator>()
		: nullptr;

	if (BattleCoordinator.IsValid())
	{
		TargetSelectionHandle = BattleCoordinator->OnTargetSelectionChanged().AddUObject(this, &ThisClass::HandleTargetSelectionChanged);
		BattleStateChangedHandle = BattleCoordinator->OnBattleStateChanged().AddUObject(
			this,
			&ThisClass::HandleBattleStateChanged);
		DefenseResultChangedHandle = BattleCoordinator->OnDefenseResultChanged().AddUObject(
			this,
			&ThisClass::HandleDefenseResultChanged);
		BindActiveCombatants();
	}

	RefreshBattleView();
	BP_OnDefenseResultUpdated(BattleCoordinator.IsValid()
		? BattleCoordinator->GetLastDefenseResult()
		: EBattleDefenseResult::None);
}

void UCombatHUDWidget::NativeDestruct()
{
	if (BattleCoordinator.IsValid()) { BattleCoordinator->OnTargetSelectionChanged().Remove(TargetSelectionHandle); }
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(DefenseFeedbackTimer); }
	ResetCommandMenuMotion();
	UnbindActiveCombatants();

	if (BattleCoordinator.IsValid() && BattleStateChangedHandle.IsValid())
	{
		BattleCoordinator->OnBattleStateChanged().Remove(BattleStateChangedHandle);
	}
	if (BattleCoordinator.IsValid() && DefenseResultChangedHandle.IsValid())
	{
		BattleCoordinator->OnDefenseResultChanged().Remove(DefenseResultChangedHandle);
	}
	BattleStateChangedHandle.Reset();
	DefenseResultChangedHandle.Reset();
	BattleCoordinator.Reset();

	Super::NativeDestruct();
}

bool UCombatHUDWidget::RequestPlayerCommand(const EBattlePlayerCommand Command)
{
	return BattleCoordinator.IsValid() && BattleCoordinator->BeginCommandSelection(Command);
}

bool UCombatHUDWidget::RequestBasicAttack()
{
	return RequestPlayerCommand(EBattlePlayerCommand::BasicAttack);
}

bool UCombatHUDWidget::RequestPowerStrike()
{
	return RequestPlayerCommand(EBattlePlayerCommand::PowerStrike);
}

bool UCombatHUDWidget::RequestDefenseCommand(const EBattleDefenseCommand Command)
{
	return BattleCoordinator.IsValid() && BattleCoordinator->RequestDefenseCommand(Command);
}

bool UCombatHUDWidget::RequestParry()
{
	return RequestDefenseCommand(EBattleDefenseCommand::Parry);
}

bool UCombatHUDWidget::RequestDodge()
{
	return RequestDefenseCommand(EBattleDefenseCommand::Dodge);
}

void UCombatHUDWidget::HandleBattleStateChanged(
	const EBattleState PreviousState,
	const EBattleState NewState)
{
	if (NewState == EBattleState::PlayerCommand && PreviousState != NewState) { MenuPage = 0; SelectedMenuEntry=1; }
	ResetCommandMenuMotion();
	BindActiveCombatants();
	RefreshBattleView();
}

void UCombatHUDWidget::HandleDefenseResultChanged(const EBattleDefenseResult Result)
{
	if (TXT_DefenseResult && GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DefenseFeedbackTimer);
		const bool bParry = Result == EBattleDefenseResult::ParrySuccess;
		const bool bDodge = Result == EBattleDefenseResult::DodgeSuccess;
		if (!bParry && !bDodge) { HideDefenseResult(); }
		else
		{
			TXT_DefenseResult->SetText(bParry ? NSLOCTEXT("CombatHUD", "Parry", "쳐내기") : NSLOCTEXT("CombatHUD", "Dodge", "회피"));
			TXT_DefenseResult->SetColorAndOpacity(FSlateColor(bParry ? FLinearColor(0.2f, 0.8f, 1.0f)
				: FLinearColor(0.5f, 1.0f, 0.5f)));
			TXT_DefenseResult->SetVisibility(ESlateVisibility::HitTestInvisible);
			GetWorld()->GetTimerManager().SetTimer(DefenseFeedbackTimer, this, &ThisClass::HideDefenseResult, 1.2f, false);
		}
	}
	BP_OnDefenseResultUpdated(Result);
}

void UCombatHUDWidget::HideDefenseResult()
{
	if (TXT_DefenseResult) { TXT_DefenseResult->SetVisibility(ESlateVisibility::Collapsed); }
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
	SetVisibility(BattleState == EBattleState::Exploration || BattleState==EBattleState::Victory || BattleState==EBattleState::Defeat
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
	// Keep old Blueprint bindings valid while replacing their visual presentation.
	for (const TCHAR* Name : {TEXT("TXT_PlayerHealth"),TEXT("PB_PlayerHealth"),TEXT("TXT_EnemyHealth"),TEXT("PB_EnemyHealth")})
	{
		if (UWidget* Health=GetWidgetFromName(Name)) { Health->SetVisibility(ESlateVisibility::Collapsed); }
	}
	RefreshCommandMenu();
}

void UCombatHUDWidget::SelectMenuEntry(int32 Entry)
{
	if (BattleCoordinator.IsValid() && (BattleCoordinator->IsSelectingTarget() || BattleCoordinator->IsConfirmingTarget())) { return; }
	if (!bCommandMenuPresentationEnabled) { return; }
	if (const UWidget* Menu = GetWidgetFromName(TEXT("CN_CommandMenu")))
	{
		if (!Menu->GetIsEnabled() || Menu->GetRenderOpacity() <= 0) { return; }
	}
	if (!BattleCoordinator.IsValid() || BattleCoordinator->GetCurrentState() != EBattleState::PlayerCommand) { return; }
	const int32 PreviousPage=MenuPage;
	if (Entry>=1 && Entry<=(MenuPage==0?3:MenuPage==1?ItemButtons.Num():MenuCharacter.IsValid()?SkillButtons.Num():0)) { SelectedMenuEntry=Entry; RefreshMenuSelection(); }
	auto* Presentation=GetOwningPlayer()?GetOwningPlayer()->FindComponentByClass<UBattlePresentationComponent>():nullptr;
	const auto* Profile=BattleCoordinator->GetCharacterActions();
 if (MenuPage==2 && ((Profile && Profile->Skills.IsValidIndex(Entry-1) && BattleCoordinator->IsActionExhausted(Profile->Skills[Entry-1]))
  || (!Profile && Entry==2 && BattleCoordinator->IsCommandExhausted(EBattlePlayerCommand::PowerStrike))))
	{
		if (Presentation) { Presentation->PlayUIAudio(EBattleUIAudio::NoUses); } return;
	}
	if (Presentation)
	{
		if (MenuPage==0 && Entry==1) { Presentation->PlayUIAudio(EBattleUIAudio::ItemMenu); }
		if (MenuPage==0 && Entry==2) { Presentation->PlayUIAudio(EBattleUIAudio::SkillMenu); }
	}
	if (MenuPage == 0)
	{
		if (Entry == 1) { MenuPage = 1; }
		else if (Entry == 2) { MenuPage = 2; }
		else if (Entry == 3) { RequestPlayerCommand(EBattlePlayerCommand::MeleeAttack); }
	}
	else if (MenuPage == 1)
 {
  const auto Items=BattleCoordinator->GetAvailableItems();
  if (Items.IsValidIndex(Entry-1)) { BattleCoordinator->BeginItemSelection(Items[Entry-1]); }
 }
	else if (MenuPage == 2)
	{
  if (Profile && Profile->Skills.IsValidIndex(Entry-1)) { BattleCoordinator->BeginActionSelection(Profile->Skills[Entry-1]); }
  else if (!Profile && Entry == 1) { RequestBasicAttack(); }
  else if (!Profile && Entry == 2) { RequestPowerStrike(); }
	}
	if (PreviousPage!=MenuPage) { SelectedMenuEntry=1; }
	RefreshCommandMenu();
}

bool UCombatHUDWidget::CanNavigateCommandMenu() const
{
	const UWidget* Menu=GetWidgetFromName(TEXT("CN_CommandMenu"));
	return bCommandMenuPresentationEnabled && BattleCoordinator.IsValid()
		&& BattleCoordinator->GetCurrentState()==EBattleState::PlayerCommand
		&& !BattleCoordinator->IsSelectingTarget() && !BattleCoordinator->IsConfirmingTarget()
		&& Menu && Menu->IsVisible() && Menu->GetIsEnabled() && Menu->GetRenderOpacity()>0;
}

TArray<UButton*> UCombatHUDWidget::GetCurrentMenuButtons() const
{
	TArray<UButton*> Buttons;
	const auto Add=[&](const TCHAR* Name) { Buttons.Add(Cast<UButton>(GetWidgetFromName(Name))); };
	if (MenuPage==0) { Add(TEXT("BTN_Items")); Add(TEXT("BTN_Skills")); Add(TEXT("BTN_Melee")); }
	else if (MenuPage==1) { for (UCombatMenuEntryButton* Button:ItemButtons) { Buttons.Add(Button); } Add(TEXT("BTN_MenuBack")); }
	else
 {
  if (MenuCharacter.IsValid()) { for (UCombatMenuEntryButton* Button:SkillButtons) { Buttons.Add(Button); } }
  else { Add(TEXT("BTN_WeakMagic")); Add(TEXT("BTN_StrongMagic")); }
  Add(TEXT("BTN_MenuBack"));
 }
	return Buttons;
}

void UCombatHUDWidget::RefreshMenuSelection()
{
	const auto Buttons=GetCurrentMenuButtons();
	if (!Buttons.IsValidIndex(SelectedMenuEntry-1) || !Buttons[SelectedMenuEntry-1] || !Buttons[SelectedMenuEntry-1]->GetIsEnabled())
	{
		SelectedMenuEntry=1;
		for (int32 I=0;I<Buttons.Num();++I)
		{
			if (Buttons[I] && Buttons[I]->GetIsEnabled()) { SelectedMenuEntry=I+1; break; }
		}
	}
	const UButton* Selected=Buttons.IsValidIndex(SelectedMenuEntry-1)?Buttons[SelectedMenuEntry-1]:nullptr;
	const float Scale=FMath::IsFinite(MenuSelectionScale)?FMath::Clamp(MenuSelectionScale,1.f,1.3f):1.12f;
 for (UCombatMenuEntryButton* Button:SkillButtons) { Button->SetRenderScale(FVector2D(Button==Selected?Scale:1.f)); }
 for (UCombatMenuEntryButton* Button:ItemButtons) { Button->SetRenderScale(FVector2D(Button==Selected?Scale:1.f)); }
 if (ItemList && MenuPage==1 && ItemButtons.IsValidIndex(SelectedMenuEntry-1)) { ItemList->ScrollWidgetIntoView(ItemButtons[SelectedMenuEntry-1],true); }
 if (SkillList && MenuPage==2 && SkillButtons.IsValidIndex(SelectedMenuEntry-1)) { SkillList->ScrollWidgetIntoView(SkillButtons[SelectedMenuEntry-1],true); }
 for (const auto& Entry:MenuButtonBaseScales)
	{
		if (UWidget* Button=GetWidgetFromName(Entry.Key))
		{
			Button->SetRenderScale(Entry.Value*(Button==Selected?Scale:1.f));
		}
	}
}

bool UCombatHUDWidget::MoveMenuSelection(int32 Direction)
{
	if (!CanNavigateCommandMenu() || Direction==0) { return false; }
	const auto Buttons=GetCurrentMenuButtons();
	if (Buttons.IsEmpty()) { return false; }
	int32 Index=FMath::Clamp(SelectedMenuEntry-1,0,Buttons.Num()-1);
	for (int32 I=0;I<Buttons.Num();++I)
	{
		Index=(Index+(Direction>0?1:Buttons.Num()-1))%Buttons.Num();
		if (Buttons[Index] && Buttons[Index]->GetIsEnabled())
		{
			const bool bSelectionChanged=SelectedMenuEntry!=Index+1;
			SelectedMenuEntry=Index+1;
			RefreshMenuSelection();
			if (bSelectionChanged)
			{
				if (auto* Presentation=GetOwningPlayer()?GetOwningPlayer()->FindComponentByClass<UBattlePresentationComponent>():nullptr)
				{
					Presentation->PlayUIAudio(EBattleUIAudio::SkillMenu);
				}
			}
			return true;
		}
	}
	return false;
}

bool UCombatHUDWidget::ConfirmMenuSelection()
{
	if (!CanNavigateCommandMenu()) { return false; }
	const auto Buttons=GetCurrentMenuButtons();
	if (!Buttons.IsValidIndex(SelectedMenuEntry-1) || !Buttons[SelectedMenuEntry-1] || !Buttons[SelectedMenuEntry-1]->GetIsEnabled()) { return false; }
	if (MenuPage!=0 && SelectedMenuEntry==Buttons.Num()) { BackToMainMenu(); }
	else { SelectMenuEntry(SelectedMenuEntry); }
	return true;
}

void UCombatHUDWidget::BackToMainMenu()
{
	if (!BattleCoordinator.IsValid() || BattleCoordinator->GetCurrentState()!=EBattleState::PlayerCommand
		|| BattleCoordinator->IsConfirmingTarget()) { return; }
	auto* Presentation=GetOwningPlayer()?GetOwningPlayer()->FindComponentByClass<UBattlePresentationComponent>():nullptr;
	if (BattleCoordinator->IsSelectingTarget())
	{
		const EBattleUIAudio Cue=BattleCoordinator->IsSelectingAlly()?EBattleUIAudio::ItemMenu:EBattleUIAudio::SkillMenu;
		if (BattleCoordinator->CancelTargetSelection() && Presentation) { Presentation->PlayUIAudio(Cue); }
		return;
	}
	if (MenuPage==0) { return; }
	SelectedMenuEntry=MenuPage==1?1:2;
	MenuPage = 0;
	RefreshCommandMenu();
	if (Presentation) { Presentation->PlayUIAudio(EBattleUIAudio::SkillMenu); }
}

void UCombatHUDWidget::ClickItems() { if (MenuPage == 0) { SelectMenuEntry(1); } }
void UCombatHUDWidget::ClickSkills() { if (MenuPage == 0) { SelectMenuEntry(2); } }
void UCombatHUDWidget::ClickMelee() { if (MenuPage == 0) { SelectMenuEntry(3); } }
void UCombatHUDWidget::ClickWeak() { if (MenuPage == 2) { SelectMenuEntry(1); } }
void UCombatHUDWidget::ClickStrong() { if (MenuPage == 2) { SelectMenuEntry(2); } }
void UCombatHUDWidget::ClickPotion() { if (MenuPage == 1) { SelectMenuEntry(1); } }
void UCombatHUDWidget::ClickBack() { BackToMainMenu(); }

FReply UCombatHUDWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (BattleCoordinator.IsValid() && BattleCoordinator->GetCurrentState() == EBattleState::PlayerCommand)
	{
		const FKey Key = Event.GetKey();
		if (!BattleCoordinator->IsSelectingTarget() && !BattleCoordinator->IsConfirmingTarget())
		{
			if (Key==EKeys::W || Key==EKeys::S)
			{
				if (!Event.IsRepeat()) { MoveMenuSelection(Key==EKeys::W?-1:1); }
				return FReply::Handled();
			}
			if (Key==EKeys::SpaceBar)
			{
				if (!Event.IsRepeat()) { ConfirmMenuSelection(); }
				return FReply::Handled();
			}
		}
		if (Key == EKeys::One || Key == EKeys::Two || Key == EKeys::Three)
		{
			if (!Event.IsRepeat()) { SelectMenuEntry(Key == EKeys::One ? 1 : Key == EKeys::Two ? 2 : 3); }
			return FReply::Handled();
		}
		if (Key == EKeys::BackSpace || Key == EKeys::Escape)
		{
			if (!Event.IsRepeat()) { BackToMainMenu(); }
			return FReply::Handled();
		}
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UCombatHUDWidget::RefreshCommandMenu()
{
	UWidget* Root = GetWidgetFromName(TEXT("CN_CommandMenu"));
	if (!Root) { return; }
	// Retain legacy Blueprint variables/events for compatibility, but replace their visible controls.
	if (UWidget* Legacy = GetWidgetFromName(TEXT("HB_CommandButtons"))) { Legacy->SetVisibility(ESlateVisibility::Collapsed); }
	if (UWidget* LegacyBorder = GetWidgetFromName(TEXT("BDR_CommandPanel"))) { LegacyBorder->SetVisibility(ESlateVisibility::Collapsed); }
	const bool bCommand = bCommandMenuPresentationEnabled && BattleCoordinator.IsValid() && BattleCoordinator->GetCurrentState() == EBattleState::PlayerCommand && !BattleCoordinator->IsSelectingTarget() && !BattleCoordinator->IsConfirmingTarget();
	Root->SetVisibility(bCommand ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	const TCHAR* Pages[] = { TEXT("CN_MainMenu"), TEXT("CN_ItemMenu"), TEXT("CN_SkillMenu") };
	for (int32 I = 0; I < 3; ++I)
	{
		if (UWidget* Page = GetWidgetFromName(Pages[I])) { Page->SetVisibility(MenuPage == I ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
	}
	if (UWidget* Back = GetWidgetFromName(TEXT("BTN_MenuBack"))) { Back->SetVisibility(MenuPage ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
	const auto Text = [this](const TCHAR* Name, const FString& Value)
	{
		if (auto* Label = Cast<UTextBlock>(GetWidgetFromName(Name))) { Label->SetText(FText::FromString(Value)); }
	};
	Text(TEXT("TXT_MenuTitle"), MenuPage == 0 ? TEXT("C O M M A N D") : MenuPage == 1 ? TEXT("I T E M S") : TEXT("S K I L L S"));
	if (BattleCoordinator.IsValid())
	{
		const auto Enabled = [this](const TCHAR* Name, EBattlePlayerCommand Command)
		{
			if (UWidget* Button = GetWidgetFromName(Name)) { Button->SetIsEnabled(BattleCoordinator->CanRequestPlayerCommand(Command)); }
		};
		Enabled(TEXT("BTN_WeakMagic"), EBattlePlayerCommand::BasicAttack);
		Enabled(TEXT("BTN_StrongMagic"), EBattlePlayerCommand::PowerStrike);
		if (UWidget* Strong=GetWidgetFromName(TEXT("BTN_StrongMagic")))
		{
			const bool bExhausted=BattleCoordinator->IsCommandExhausted(EBattlePlayerCommand::PowerStrike);
			Strong->SetIsEnabled(true); Strong->SetRenderOpacity(bExhausted?.45f:1.f);
		}
		Enabled(TEXT("BTN_Potion"), EBattlePlayerCommand::UsePotion);
		Enabled(TEXT("BTN_Melee"), EBattlePlayerCommand::MeleeAttack);
		Text(TEXT("TXT_Potion"), FString::Printf(TEXT("[1]  회복 물약   × %d\n\nHP %.0f 회복 · 턴 소모"), BattleCoordinator->GetRemainingPotions(), BattleCoordinator->GetPotionHealing()));


	}
	RefreshCharacterSkills();
	RefreshItems();
	RefreshMenuSelection();
	if (bCommand) { UpdateMenuAnchor(); }
}

void UCombatHUDWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	// Continuous bone/camera projection requires frame updates, only while this menu is shown.
	if (bCommandMenuPresentationEnabled && BattleCoordinator.IsValid()
		&& BattleCoordinator->GetCurrentState() == EBattleState::PlayerCommand && !BattleCoordinator->IsSelectingTarget() && !BattleCoordinator->IsConfirmingTarget()) { UpdateMenuAnchor(DeltaTime); }
}

void UCombatHUDWidget::HandleTargetSelectionChanged()
{
	ResetCommandMenuMotion();
	RefreshBattleView();
}

int32 UCombatHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	Layer = Super::NativePaint(Args, Geometry, Culling, Elements, Layer, Style, bEnabled);
	Layer = PaintHealthHUD(Geometry,Elements,Layer);
	if (!BattleCoordinator.IsValid() || !BattleCoordinator->IsSelectingTarget()) { return Layer; }
	const FVector2D Size = Geometry.GetLocalSize();
	for (AActor* Target:BattleCoordinator->GetSelectedTargets())
	{
	const FVector Marker=UCombatantVisualData::ResolveAnchor(Target,ECombatantAnchor::TargetMarker);
	FVector2D Screen;
	if (GetOwningPlayer()->ProjectWorldLocationToScreen(Marker, Screen, true))
	{
		// Normalize player-viewport pixels into this full-screen HUD's local size.
		int32 Width, Height; GetOwningPlayer()->GetViewportSize(Width, Height);
		if (Width <= 0 || Height <= 0) { return Layer; }
		const FVector2D Center = Screen * Size / FVector2D(Width, Height);
		TArray<FVector2D> Diamond = {Center+FVector2D(0,-15),Center+FVector2D(15,0),Center+FVector2D(0,15),Center+FVector2D(-15,0),Center+FVector2D(0,-15)};
		FSlateDrawElement::MakeLines(Elements, ++Layer, Geometry.ToPaintGeometry(), Diamond, ESlateDrawEffect::None, FLinearColor(1,.85f,.25f), true, 3);
	}
	}
	return Layer;
}

void UCombatHUDWidget::SetCommandMenuPresentationEnabled(bool bEnabled)
{
	bCommandMenuPresentationEnabled = bEnabled;
	ResetCommandMenuMotion();
	RefreshCommandMenu();
}

void UCombatHUDWidget::ResetCommandMenuMotion()
{
	bMotionReferenceReady = false;
	MotionPlayer.Reset();
	LastViewTarget.Reset();
	LastViewSize = FVector2D::ZeroVector;
	SmoothedSway = FVector2D::ZeroVector;
	SmoothedSwayAngle = 0;
	if (UWidget* Menu = GetWidgetFromName(TEXT("CN_CommandMenu"))) { Menu->SetRenderTransformAngle(MenuBaseAngle); }
}

void UCombatHUDWidget::UpdateMenuAnchor(float DeltaTime)
{
	UWidget* Root = GetWidgetFromName(TEXT("CN_CommandMenu"));
	APawn* Player = BattleCoordinator.IsValid() ? BattleCoordinator->GetActivePlayerPawn() : nullptr;
	if (!Root || !Player || !GetOwningPlayer()) { return; }
	FVector2D Position;
	const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), Player->GetActorLocation() + (UCombatantVisualData::Find(Player)?UCombatantVisualData::Find(Player)->MenuWorldOffset:FVector::ZeroVector), Position, true);
	const FVector2D ViewSize = UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(GetOwningPlayer()).GetLocalSize();
	if (ViewSize.X <= 0 || ViewSize.Y <= 0) { return; }
	APlayerController* PC = GetOwningPlayer();
	APlayerCameraManager* Camera = PC->PlayerCameraManager;
	if (!bProjected)
	{
		// Do not show a command panel floating at screen center when the subject is behind the camera.
		Root->SetRenderOpacity(0);
		Root->SetIsEnabled(false);
		ResetCommandMenuMotion();
		return;
	}
	Root->SetRenderOpacity(1);
	Root->SetIsEnabled(true);
	if (Camera)
	{
		const FVector CameraPosition = Camera->GetCameraLocation();
		const FRotator CameraRotation = Camera->GetCameraRotation();
		const float FOV = Camera->GetFOVAngle();
		const FRotator RotationDelta = (CameraRotation - LastCameraRotation).GetNormalized();
		const bool bCut = MotionPlayer.Get() != Player || LastViewTarget.Get() != PC->GetViewTarget()
			|| !LastViewSize.Equals(ViewSize, 0.5) || FVector::DistSquared(CameraPosition, LastCameraPosition) > FMath::Square(150.0f)
			|| FMath::Abs(RotationDelta.Yaw) > 20 || FMath::Abs(RotationDelta.Pitch) > 20 || FMath::Abs(FOV - LastCameraFOV) > 8;
		if (bCut) { ResetCommandMenuMotion(); }
		MotionPlayer = Player;
		LastViewTarget = PC->GetViewTarget();
		LastViewSize = ViewSize;
		LastCameraPosition = CameraPosition;
		LastCameraRotation = CameraRotation;
		LastCameraFOV = FOV;
	}
	FVector2D TargetSway = FVector2D::ZeroVector;
	float TargetAngle = 0;
	const ACharacter* Character = Cast<ACharacter>(Player);
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	const auto* Visual=UCombatantVisualData::Find(Player);
	if (DeltaTime > 0 && Mesh && Visual && Mesh->DoesSocketExist(Visual->MenuMotionBone) && Mesh->DoesSocketExist(Visual->MenuReferenceBone))
	{
		const FTransform ActorTransform = Player->GetActorTransform();
		const FVector Body = ActorTransform.InverseTransformPosition(Mesh->GetSocketLocation(Visual->MenuMotionBone));
		const FVector Pelvis = ActorTransform.InverseTransformPosition(Mesh->GetSocketLocation(Visual->MenuReferenceBone));
		if (!bMotionReferenceReady)
		{
			ReferenceBodyPosition = Body;
			ReferencePelvisPosition = Pelvis;
			bMotionReferenceReady = true;
		}
		FVector2D CurrentBody, RefBody, CurrentPelvis, RefPelvis;
		const auto Project = [&](const FVector& Local, FVector2D& Screen)
		{
			return UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, ActorTransform.TransformPosition(Local), Screen, true);
		};
		if (Project(Body, CurrentBody) && Project(ReferenceBodyPosition, RefBody)
			&& Project(Pelvis, CurrentPelvis) && Project(ReferencePelvisPosition, RefPelvis))
		{
			TargetSway = (CurrentBody - RefBody) * FMath::Clamp(MenuSwayStrength, 0.0f, 5.0f);
			const double Limit = FMath::Clamp(MenuMaxSway, 0.0f, 20.0f);
			TargetSway = TargetSway.GetClampedToMaxSize(Limit);
			const FVector2D CurrentUp = CurrentBody - CurrentPelvis, RefUp = RefBody - RefPelvis;
			if (CurrentUp.SizeSquared() > 4 && RefUp.SizeSquared() > 4)
			{
				const float Angle = FMath::RadiansToDegrees(FMath::Atan2(CurrentUp.Y, CurrentUp.X) - FMath::Atan2(RefUp.Y, RefUp.X));
				const float AngleLimit = FMath::Clamp(MenuMaxSwayAngle, 0.0f, 2.0f);
				TargetAngle = FMath::Clamp(FRotator::NormalizeAxis(Angle), -AngleLimit, AngleLimit);
			}
		}
	}
	const float Alpha = 1 - FMath::Exp(-FMath::Clamp(MenuMotionFollowSpeed, 0.1f, 30.0f) * FMath::Max(0.0f, DeltaTime));
	SmoothedSway = FMath::Lerp(SmoothedSway, TargetSway, Alpha);
	SmoothedSwayAngle = FMath::Lerp(SmoothedSwayAngle, TargetAngle, Alpha);
	Position += Visual?Visual->MenuScreenOffset:FVector2D::ZeroVector;
	Position += SmoothedSway;
	Root->SetRenderTransformAngle(MenuBaseAngle + SmoothedSwayAngle);
	Position.X = FMath::Clamp(Position.X, 24.0, FMath::Max(24.0, ViewSize.X - 560));
	// Reserve space for the visible page, rather than the taller skill page on every menu.
	const auto VisibleBottom = [](auto&& Self, const UCanvasPanel* Canvas) -> double
	{
		double Bottom = 0;
		if (!Canvas) { return Bottom; }
		for (const UWidget* Child : Canvas->GetAllChildren())
		{
			if (!Child || Child->GetVisibility() == ESlateVisibility::Collapsed || Child->GetVisibility() == ESlateVisibility::Hidden) { continue; }
			const auto* ChildSlot = Cast<UCanvasPanelSlot>(Child->Slot);
			if (!ChildSlot) { continue; }
			const auto* ChildCanvas = Cast<UCanvasPanel>(Child);
			const double Height = ChildCanvas ? Self(Self, ChildCanvas) : ChildSlot->GetSize().Y;
			Bottom = FMath::Max(Bottom, ChildSlot->GetPosition().Y + Height);
		}
		return Bottom;
	};
	const double MenuHeight = VisibleBottom(VisibleBottom, Cast<UCanvasPanel>(Root)) + 20.0;
	Position.Y = FMath::Clamp(Position.Y, 130.0, FMath::Max(130.0, ViewSize.Y - MenuHeight));
	if (auto* MenuSlot = Cast<UCanvasPanelSlot>(Root->Slot)) { MenuSlot->SetPosition(Position); }
}

void UCombatHUDWidget::RefreshCharacterSkills()
{
 UCharacterCombatData* Profile=BattleCoordinator.IsValid()?BattleCoordinator->GetCharacterActions():nullptr;
 const bool bRebuild=Profile!=MenuCharacter.Get() || (Profile && SkillButtons.Num()!=Profile->Skills.Num());
 if (bRebuild)
 {
  if (SkillList) { SkillList->RemoveFromParent(); SkillList=nullptr; }
  SkillButtons.Reset(); MenuCharacter=Profile;
  auto* Canvas=Cast<UCanvasPanel>(GetWidgetFromName(TEXT("CN_SkillMenu")));
  auto* Template=Cast<UButton>(GetWidgetFromName(TEXT("BTN_WeakMagic")));
  if (Profile && Canvas && WidgetTree)
  {
   SkillList=WidgetTree->ConstructWidget<UScrollBox>();
   auto* CanvasSlot=Canvas->AddChildToCanvas(SkillList);
   CanvasSlot->SetPosition(FVector2D(0,0)); CanvasSlot->SetSize(FVector2D(530,260));
   for (int32 I=0;I<Profile->Skills.Num();++I)
   {
    auto* Button=WidgetTree->ConstructWidget<UCombatMenuEntryButton>();
    if (Template) { Button->SetStyle(Template->GetStyle()); }
    Button->SetRenderTransformPivot(FVector2D(.5f,.5f)); Button->SetPixelSnapping(EWidgetPixelSnapping::Disabled);
    auto* Label=WidgetTree->ConstructWidget<UTextBlock>();
    FSlateFontInfo Font=GetDefault<UTextBlock>()->GetFont();
    if (const auto* TemplateLabel=Cast<UTextBlock>(GetWidgetFromName(TEXT("TXT_WeakMagic")))) { Font=TemplateLabel->GetFont(); }
    Font.Size=17;
    Label->SetFont(Font); Label->SetAutoWrapText(false);
    Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    Button->AddChild(Label); Button->BindEntry(this,I+1);
    auto* Row=WidgetTree->ConstructWidget<USizeBox>();
    Row->SetHeightOverride(105); Row->AddChild(Button);
    auto* ListSlot=Cast<UScrollBoxSlot>(SkillList->AddChild(Row));
    ListSlot->SetPadding(FMargin(30,9,30,9)); ListSlot->SetHorizontalAlignment(HAlign_Fill);
    SkillButtons.Add(Button);
   }
  }
 }
 for (const TCHAR* Name:{TEXT("BTN_WeakMagic"),TEXT("BTN_StrongMagic")})
 { if (auto* Widget=GetWidgetFromName(Name)) { Widget->SetVisibility(Profile?ESlateVisibility::Collapsed:ESlateVisibility::Visible); } }
 if (!Profile) { return; }
 for (int32 I=0;I<SkillButtons.Num();++I)
 {
  const UCharacterActionData* Action=Profile->Skills[I].Get(); UCombatMenuEntryButton* Button=SkillButtons[I].Get();
  Button->SetRenderOpacity(Action && BattleCoordinator->IsActionExhausted(Profile->Skills[I])?.45f:1.f);
  if (auto* Label=Cast<UTextBlock>(Button->GetContent()); Action && Label)
  {
   Label->SetText(FText::FromString(FString::Printf(TEXT("[%d]  %s\n%s · 피해 %.0f"),I+1,*Action->DisplayName.ToString(),*Action->Description.ToString(),Action->BaseDamage)));
  }
 }
}

void UCombatHUDWidget::RefreshItems()
{
 const auto Items=BattleCoordinator.IsValid()?BattleCoordinator->GetAvailableItems():TArray<UCombatItemData*>();
 if (!ItemList || ItemButtons.Num()!=Items.Num())
 {
  if (ItemList) { ItemList->RemoveFromParent();ItemList=nullptr; }
  ItemButtons.Reset();
  auto* Canvas=Cast<UCanvasPanel>(GetWidgetFromName(TEXT("CN_ItemMenu")));
  auto* Template=Cast<UButton>(GetWidgetFromName(TEXT("BTN_Potion")));
  if (Canvas && WidgetTree)
  {
   ItemList=WidgetTree->ConstructWidget<UScrollBox>();auto* CanvasSlot=Canvas->AddChildToCanvas(ItemList);
   CanvasSlot->SetSize(FVector2D(530,260));
   for (int32 I=0;I<Items.Num();++I)
   {
    auto* Button=WidgetTree->ConstructWidget<UCombatMenuEntryButton>();if (Template) { Button->SetStyle(Template->GetStyle()); }
    Button->SetRenderTransformPivot(FVector2D(.5f,.5f));Button->SetPixelSnapping(EWidgetPixelSnapping::Disabled);
    auto* Label=WidgetTree->ConstructWidget<UTextBlock>();
    FSlateFontInfo Font=GetDefault<UTextBlock>()->GetFont();
    if (const auto* TemplateLabel=Cast<UTextBlock>(GetWidgetFromName(TEXT("TXT_Potion")))) { Font=TemplateLabel->GetFont(); }
    Font.Size=17;Label->SetFont(Font);Label->SetAutoWrapText(true);Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    Button->AddChild(Label);Button->BindEntry(this,I+1);
    auto* Row=WidgetTree->ConstructWidget<USizeBox>();Row->SetHeightOverride(105);Row->AddChild(Button);
    auto* ListSlot=Cast<UScrollBoxSlot>(ItemList->AddChild(Row));ListSlot->SetPadding(FMargin(30,9,30,9));ListSlot->SetHorizontalAlignment(HAlign_Fill);
    ItemButtons.Add(Button);
   }
  }
 }
 if (auto* Legacy=GetWidgetFromName(TEXT("BTN_Potion"))) { Legacy->SetVisibility(ESlateVisibility::Collapsed); }
 for (int32 I=0;I<ItemButtons.Num();++I)
 {
  const auto* Item=Items[I];UCombatMenuEntryButton* Button=ItemButtons[I].Get();
  Button->SetIsEnabled(BattleCoordinator->CanUseItem(Items[I]));
  if (auto* Label=Cast<UTextBlock>(Button->GetContent()))
  { Label->SetText(FText::FromString(FString::Printf(TEXT("[%d]  %s  × %d\nHP %.0f 회복 · 턴 소모"),I+1,*Item->DisplayName.ToString(),BattleCoordinator->GetRemainingItems(Items[I]),Item->HealingAmount))); }
 }
}
