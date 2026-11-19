// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Combat/BattleDefense.h"
#include "Combat/BattlePlayerCommand.h"
#include "Combat/BattleState.h"
#include "CoreMinimal.h"
#include "CombatHUDWidget.generated.h"

class UBattleCoordinator;
class UCombatantComponent;
class UTextBlock;
class UButton;
class UCharacterCombatData;
class UCombatMenuEntryButton;
class UScrollBox;

UCLASS(Abstract, Blueprintable)
class EXPEDITIONPORTFOLIO_API UCombatHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Combat|Health HUD") TObjectPtr<class UCombatHealthHUDData> HealthHUDData;
	UFUNCTION(BlueprintCallable, Category = "Combat|Commands")
	bool RequestPlayerCommand(EBattlePlayerCommand Command);

	UFUNCTION(BlueprintCallable, Category = "Combat|Commands")
	bool RequestBasicAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat|Commands")
	bool RequestPowerStrike();

	UFUNCTION(BlueprintCallable, Category = "Combat|Commands")
	bool RequestDefenseCommand(EBattleDefenseCommand Command);

	UFUNCTION(BlueprintCallable, Category = "Combat|Commands")
	bool RequestParry();

	UFUNCTION(BlueprintCallable, Category = "Combat|Commands")
	bool RequestDodge();

	// Presentation navigation only: commands are still validated by the Coordinator.
	UFUNCTION(BlueprintCallable, Category="Combat|Menu") void SelectMenuEntry(int32 Entry);
	UFUNCTION(BlueprintCallable, Category="Combat|Menu") void BackToMainMenu();
	UFUNCTION(BlueprintPure, Category="Combat|Menu") int32 GetMenuPage() const { return MenuPage; }
	UFUNCTION(BlueprintCallable, Category="Combat|Menu") bool MoveMenuSelection(int32 Direction);
	UFUNCTION(BlueprintCallable, Category="Combat|Menu") bool ConfirmMenuSelection();
	UFUNCTION(BlueprintPure, Category="Combat|Menu") int32 GetSelectedMenuEntry() const { return SelectedMenuEntry; }
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Menu", meta=(ClampMin="1",ClampMax="1.3")) float MenuSelectionScale = 1.12f;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") FVector2D MenuScreenOffset = FVector2D(90, -130);
	UPROPERTY(BlueprintReadOnly,Category="Legacy") FName MenuMotionBone = TEXT("spine_03");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Menu Motion", meta=(ClampMin="0", ClampMax="5")) float MenuSwayStrength = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Menu Motion", meta=(ClampMin="0", ClampMax="20")) float MenuMaxSway = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Menu Motion", meta=(ClampMin="0", ClampMax="2")) float MenuMaxSwayAngle = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Menu Motion", meta=(ClampMin="0.1", ClampMax="30")) float MenuMotionFollowSpeed = 2.5f;
	// Presentation gate for future camera transitions. This never changes battle/input rules.
	UFUNCTION(BlueprintCallable, Category="Combat|Menu") void SetCommandMenuPresentationEnabled(bool bEnabled);
	UFUNCTION(BlueprintCallable, Category="Combat|Menu") void ResetCommandMenuMotion();

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_DefenseResult;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Presentation", meta = (DisplayName = "Battle View Updated"))
	void BP_OnBattleViewUpdated(
		EBattleState BattleState,
		float PlayerCurrentHealth,
		float PlayerMaxHealth,
		float EnemyCurrentHealth,
		float EnemyMaxHealth,
		bool bCanUseBasicAttack,
		bool bCanUsePowerStrike);

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Presentation", meta = (DisplayName = "Defense Result Updated"))
	void BP_OnDefenseResultUpdated(EBattleDefenseResult Result);

private:
	int32 PaintHealthHUD(const FGeometry& Geometry,FSlateWindowElementList& Elements,int32 Layer) const;
	void RefreshCharacterSkills();
	void RefreshItems();
	UScrollBox* CreateCommandList(FName CanvasName);
	UCombatMenuEntryButton* AddCommandRow(UScrollBox& List, int32 Entry, const UButton* ButtonTemplate,
		const UTextBlock* LabelTemplate, bool bWrapText);
	UPROPERTY(Transient) TObjectPtr<UScrollBox> ItemList;
	UPROPERTY(Transient) TArray<TObjectPtr<UCombatMenuEntryButton>> ItemButtons;
	TWeakObjectPtr<UCharacterCombatData> MenuCharacter;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> SkillList;
	UPROPERTY(Transient) TArray<TObjectPtr<UCombatMenuEntryButton>> SkillButtons;
	void HandleTargetSelectionChanged();
	FDelegateHandle TargetSelectionHandle;
	void HandleBattleStateChanged(EBattleState PreviousState, EBattleState NewState);
	void HandleDefenseResultChanged(EBattleDefenseResult Result);
	void HandleCombatantHealthChanged(float PreviousHealth, float NewHealth);
	void HandleCombatantDefeated();
	void BindActiveCombatants();
	void UnbindActiveCombatants();
	void RefreshBattleView();
	void RefreshHealthDisplay(EBattleState BattleState);
	void HideDefenseResult();
	void RefreshCommandMenu();
	bool CanNavigateCommandMenu() const;
	TArray<UButton*> GetCurrentMenuButtons() const;
	void RefreshMenuSelection();
	int32 SelectedMenuEntry = 1;
	TMap<FName,FVector2D> MenuButtonBaseScales;
	void UpdateMenuAnchor(float DeltaTime = 0.0f);
	UFUNCTION() void ClickItems();
	UFUNCTION() void ClickSkills();
	UFUNCTION() void ClickMelee();
	UFUNCTION() void ClickWeak();
	UFUNCTION() void ClickStrong();
	UFUNCTION() void ClickPotion();
	UFUNCTION() void ClickBack();
	int32 MenuPage = 0; // 0 main, 1 items, 2 skills; never a battle state.
	bool bCommandMenuPresentationEnabled = true;
	bool bMotionReferenceReady = false;
	FVector ReferenceBodyPosition = FVector::ZeroVector;
	FVector ReferencePelvisPosition = FVector::ZeroVector;
	FVector2D SmoothedSway = FVector2D::ZeroVector;
	float SmoothedSwayAngle = 0;
	float MenuBaseAngle = 0;
	TWeakObjectPtr<APawn> MotionPlayer;
	TWeakObjectPtr<AActor> LastViewTarget;
	FVector LastCameraPosition = FVector::ZeroVector;
	FRotator LastCameraRotation = FRotator::ZeroRotator;
	FVector2D LastViewSize = FVector2D::ZeroVector;
	float LastCameraFOV = 0;
	FTimerHandle DefenseFeedbackTimer;

	TWeakObjectPtr<UBattleCoordinator> BattleCoordinator;
	TWeakObjectPtr<UCombatantComponent> PlayerCombatant;
	TWeakObjectPtr<UCombatantComponent> EnemyCombatant;
	FDelegateHandle BattleStateChangedHandle;
	FDelegateHandle DefenseResultChangedHandle;
	FDelegateHandle PlayerHealthChangedHandle;
	FDelegateHandle PlayerDefeatedHandle;
	FDelegateHandle EnemyHealthChangedHandle;
	FDelegateHandle EnemyDefeatedHandle;
};
