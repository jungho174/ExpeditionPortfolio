#include "UI/CombatMenuEditorLibrary.h"
#include "UI/CombatHUDWidget.h"
#include "UI/CombatHealthHUDData.h"

#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Brushes/SlateColorBrush.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace
{
	template<class T> T* Widget(UWidgetBlueprint* BP, const TCHAR* Name)
	{
		T* Result = BP->WidgetTree->ConstructWidget<T>(T::StaticClass(), Name);
		Result->SetFlags(RF_Transactional);
		Result->bIsVariable = true;
		BP->OnVariableAdded(Result->GetFName());
		return Result;
	}
	void Place(UCanvasPanel* Parent, UWidget* Child, FVector2D Position, FVector2D Size)
	{
		auto* Slot = Parent->AddChildToCanvas(Child);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
	}
	UTextBlock* Label(UWidgetBlueprint* BP, const TCHAR* Name, const TCHAR* Value, int32 Size)
	{
		auto* Text = Widget<UTextBlock>(BP, Name);
		Text->SetText(FText::FromString(Value));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FLinearColor(0.94f, 0.91f, 0.81f));
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	}
	void Button(UWidgetBlueprint* BP, UCanvasPanel* Page, const TCHAR* Name, const TCHAR* TextName,
		const TCHAR* Value, FVector2D Position, FVector2D Size, int32 FontSize=20)
	{
		auto* B = Widget<UButton>(BP, Name);
		FButtonStyle Style = B->GetStyle();
		Style.SetNormal(FSlateColorBrush(FLinearColor(0.018f, 0.022f, 0.025f, 0.94f)));
		Style.SetHovered(FSlateColorBrush(FLinearColor(0.13f, 0.12f, 0.08f, 0.98f)));
		Style.SetPressed(FSlateColorBrush(FLinearColor(0.27f, 0.22f, 0.12f, 1)));
		Style.SetDisabled(FSlateColorBrush(FLinearColor(0.025f, 0.028f, 0.03f, 0.7f)));
		Style.NormalPadding = FMargin(20, 12);
		Style.PressedPadding = FMargin(22, 14, 18, 10);
		B->SetStyle(Style);
		B->SetContent(Label(BP, TextName, Value, FontSize));
		Place(Page, B, Position, Size);
	}
}
#endif

bool UCombatMenuEditorLibrary::BuildCommandLayout(UBlueprint* Blueprint)
{
#if WITH_EDITOR
	auto* BP = Cast<UWidgetBlueprint>(Blueprint);
	if (!BP || !BP->WidgetTree) { return false; }
	if (BP->WidgetTree->FindWidget(TEXT("CN_CommandMenu"))) { return true; }
	auto* Root = Cast<UCanvasPanel>(BP->WidgetTree->RootWidget);
	if (!Root) { return false; }
	BP->Modify();
	BP->WidgetTree->Modify();
	Root->Modify();
	auto* Menu = Widget<UCanvasPanel>(BP, TEXT("CN_CommandMenu"));
	Place(Root, Menu, FVector2D(900, 420), FVector2D(530, 350));
	CastChecked<UCanvasPanelSlot>(Menu->Slot)->SetZOrder(20);
	Menu->SetRenderTransformPivot(FVector2D::ZeroVector);
	Menu->SetRenderTransformAngle(-8);
	Menu->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	auto* Title = Label(BP, TEXT("TXT_MenuTitle"), TEXT("C O M M A N D"), 14);
	Title->SetColorAndOpacity(FLinearColor(0.8f, 0.65f, 0.36f));
	Place(Menu, Title, FVector2D(24, 0), FVector2D(440, 30));
	const TCHAR* Names[] = { TEXT("CN_MainMenu"), TEXT("CN_ItemMenu"), TEXT("CN_SkillMenu") };
	UCanvasPanel* Pages[3];
	for (int32 I=0; I<3; ++I)
	{
		Pages[I] = Widget<UCanvasPanel>(BP, Names[I]);
		Place(Menu, Pages[I], FVector2D(0, 35), FVector2D(530, 270));
		Pages[I]->SetVisibility(I == 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	Button(BP, Pages[0], TEXT("BTN_Items"), TEXT("TXT_Items"), TEXT("[1]   아이템"), FVector2D(0, 0), FVector2D(280, 60));
	Button(BP, Pages[0], TEXT("BTN_Skills"), TEXT("TXT_Skills"), TEXT("[2]   기술"), FVector2D(28, 78), FVector2D(280, 60));
	Button(BP, Pages[0], TEXT("BTN_Melee"), TEXT("TXT_Melee"), TEXT("[3]   공격"), FVector2D(56, 156), FVector2D(280, 60));
	Button(BP, Pages[2], TEXT("BTN_WeakMagic"), TEXT("TXT_WeakMagic"), TEXT("[1]  약한 마법\n\n불꽃 발사 · 피해 25 · 횟수 제한 없음"), FVector2D(0, 0), FVector2D(490, 105), 17);
	Button(BP, Pages[2], TEXT("BTN_StrongMagic"), TEXT("TXT_StrongMagic"), TEXT("[2]  강한 마법\n\n얼음 안개 · 피해 40 · 전투당 1회"), FVector2D(24, 125), FVector2D(490, 105), 17);
	Button(BP, Pages[1], TEXT("BTN_Potion"), TEXT("TXT_Potion"), TEXT("[1]  회복 물약\n\nHP 30 회복 · 턴 소모"), FVector2D(0, 0), FVector2D(490, 105), 17);
	Button(BP, Menu, TEXT("BTN_MenuBack"), TEXT("TXT_MenuBack"), TEXT("←  뒤로  [Backspace]"), FVector2D(45, 295), FVector2D(280, 45), 14);
	BP->WidgetTree->FindWidget(TEXT("BTN_MenuBack"))->SetVisibility(ESlateVisibility::Collapsed);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	return true;
#else
	return false;
#endif
}

bool UCombatMenuEditorLibrary::ConfigurePartyHUD(UBlueprint* Blueprint,UCombatHealthHUDData* Appearance)
{
#if WITH_EDITOR
	auto* BP=Cast<UWidgetBlueprint>(Blueprint);
	if (!BP || !BP->WidgetTree || !BP->GeneratedClass || !Appearance) { return false; }
	auto* Defaults=Cast<UCombatHUDWidget>(BP->GeneratedClass->GetDefaultObject());
	if (!Defaults) { return false; }
	BP->Modify();BP->WidgetTree->Modify();Defaults->Modify();Defaults->HealthHUDData=Appearance;
	for (const TCHAR* Name:{TEXT("TXT_PlayerHealth"),TEXT("PB_PlayerHealth"),TEXT("TXT_EnemyHealth"),TEXT("PB_EnemyHealth")})
	{
		if (auto* Widget=BP->WidgetTree->FindWidget(Name)) { Widget->Modify(); Widget->SetVisibility(ESlateVisibility::Collapsed); }
	}
	UWidget* State=BP->WidgetTree->FindWidget(TEXT("TXT_BattleState"));
	while (State)
	{
		if (auto* Size=Cast<USizeBox>(State)) { Size->Modify(); Size->ClearHeightOverride(); }
		if (auto* Border=Cast<UBorder>(State)) { Border->Modify();Border->SetPadding(FMargin(20,8)); }
		if (auto* CanvasSlot=Cast<UCanvasPanelSlot>(State->Slot))
		{
			CanvasSlot->Modify();CanvasSlot->SetAutoSize(true);CanvasSlot->SetAnchors(FAnchors(.5f,0));
			CanvasSlot->SetAlignment(FVector2D(.5f,0));CanvasSlot->SetPosition(FVector2D(0,24));break;
		}
		State=State->GetParent();
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	return State!=nullptr;
#else
	return false;
#endif
}
