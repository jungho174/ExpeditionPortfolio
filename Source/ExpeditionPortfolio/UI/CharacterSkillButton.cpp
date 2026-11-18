#include "UI/CharacterSkillButton.h"
#include "UI/CombatHUDWidget.h"
void UCombatMenuEntryButton::BindEntry(UCombatHUDWidget* HUD,int32 Index)
{ OwnerHUD=HUD; Entry=Index; OnClicked.AddUniqueDynamic(this,&ThisClass::SelectEntry); }
void UCombatMenuEntryButton::SelectEntry()
{ if (OwnerHUD.IsValid()) { OwnerHUD->SelectMenuEntry(Entry); } }
