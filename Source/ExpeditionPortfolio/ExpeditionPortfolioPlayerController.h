// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ExpeditionPortfolioPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UCombatHUDWidget;
enum class EBattleState : uint8;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class AExpeditionPortfolioPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	UPROPERTY(EditAnywhere, Category = "Input|Combat")
	TObjectPtr<UInputMappingContext> PlayerCommandMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input|Combat")
	TObjectPtr<UInputAction> BasicAttackInputAction;

	UPROPERTY(EditAnywhere, Category = "Input|Combat")
	TObjectPtr<UInputAction> PowerStrikeInputAction;

	UPROPERTY(EditAnywhere, Category = "Input|Combat")
	TObjectPtr<UInputMappingContext> DefenseMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input|Combat")
	TObjectPtr<UInputAction> ParryInputAction;

	UPROPERTY(EditAnywhere, Category = "Input|Combat")
	TObjectPtr<UInputAction> DodgeInputAction;

	UPROPERTY(EditAnywhere, Category = "Combat|UI")
	TSubclassOf<UCombatHUDWidget> CombatHUDWidgetClass;

	UPROPERTY()
	TObjectPtr<UCombatHUDWidget> CombatHUDWidget;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Battle state delegate cleanup */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	void HandleBattleStateChanged(EBattleState PreviousState, EBattleState NewState);
	void HandleBasicAttackInput();
	void HandlePowerStrikeInput();
	void HandleParryInput();
	void HandleDodgeInput();
	void SetExplorationInputEnabled(bool bEnabled);
	void SetPlayerCommandInputEnabled(bool bEnabled);
	void SetDefenseInputEnabled(bool bEnabled);
	void SetBattleInteractionEnabled(bool bEnabled);


	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	FDelegateHandle BattleStateChangedHandle;
	bool bExplorationInputEnabled = false;
	bool bPlayerCommandInputEnabled = false;
	bool bDefenseInputEnabled = false;
	bool bBattleInteractionEnabled = false;

};
