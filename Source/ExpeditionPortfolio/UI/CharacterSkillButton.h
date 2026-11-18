#pragma once
#include "CoreMinimal.h"
#include "Components/Button.h"
#include "CharacterSkillButton.generated.h"
class UCombatHUDWidget;
UCLASS()
class UCombatMenuEntryButton : public UButton
{
 GENERATED_BODY()
public:
 UCombatMenuEntryButton() { InitIsFocusable(false); }
 void BindEntry(UCombatHUDWidget* HUD,int32 Index);
private:
 UFUNCTION() void SelectEntry();
 TWeakObjectPtr<UCombatHUDWidget> OwnerHUD;
 int32 Entry=0;
};
