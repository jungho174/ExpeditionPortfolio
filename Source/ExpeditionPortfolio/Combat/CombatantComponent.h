// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "CombatantComponent.generated.h"

class UEnemyAttackPatternData;
class UPlayerCommandData;
class UCharacterCombatData;
class UCombatantVisualData;

UENUM(BlueprintType)
enum class ECombatantPersistence : uint8 { Transient, UniquePartyMember };

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnCombatantHealthChanged,
	float /* PreviousHealth */,
	float /* NewHealth */);

DECLARE_MULTICAST_DELEGATE(FOnCombatantDefeated);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnCombatantHealthChangedBlueprint,
	float, PreviousHealth,
	float, NewHealth);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatantDefeatedBlueprint);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class EXPEDITIONPORTFOLIO_API UCombatantComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Combat|Presentation") TObjectPtr<UCombatantVisualData> VisualData;
	// Enemy instances stay transient even when they share the same definition and appearance.
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Combat|Identity") ECombatantPersistence Persistence=ECombatantPersistence::Transient;
	UFUNCTION(BlueprintPure,Category="Combat|Identity") FName GetCharacterId() const;
	UFUNCTION(BlueprintPure,Category="Combat|Identity") bool HasValidIdentity() const { return Persistence==ECombatantPersistence::Transient || bIdentityRegistered; }
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Combat|Character") TObjectPtr<UCharacterCombatData> CharacterActions;
	// Select the component implementing ParryCounterProvider on this character. None chooses the first valid provider.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Combat|Legacy",meta=(EditCondition="CharacterActions == nullptr",EditConditionHides)) FName CounterProviderComponentName;
	UCombatantComponent();
	// Unique across party members and maps. None leaves enemies/non-party actors transient.
	UPROPERTY(BlueprintReadOnly,Category="Legacy") FName PersistentCharacterId;
	// Explicit snapshot restoration may revive; ordinary healing never does.
	bool RestoreBattleHealth(float Health);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat|Presentation") FText CombatDisplayName = FText::FromString(TEXT("적"));

	// Only used when this combatant is registered as the enemy. Selection belongs to the Coordinator.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Enemy Patterns")
	TArray<TObjectPtr<UEnemyAttackPatternData>> EnemyAttackPatterns;

	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	TObjectPtr<UPlayerCommandData> PlayerCommandData;

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	bool IsDefeated() const { return CurrentHealth <= 0.0f; }

	UFUNCTION(BlueprintCallable, Category = "Combat|Health")
	bool ApplyDamage(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Combat|Health")
	bool RestoreHealth(float Amount);

	FOnCombatantHealthChanged& OnHealthChanged() { return HealthChanged; }
	FOnCombatantDefeated& OnDefeated() { return Defeated; }

	UPROPERTY(BlueprintAssignable, Category = "Combat|Health")
	FOnCombatantHealthChangedBlueprint HealthChangedBlueprint;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Health")
	FOnCombatantDefeatedBlueprint DefeatedBlueprint;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	FName RegisteredCharacterId;
	bool bIdentityRegistered=false;
	void StorePersistentHealth();
	UPROPERTY(EditAnywhere, Category = "Combat|Health", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Combat|Health")
	float CurrentHealth = 100.0f;

	FOnCombatantHealthChanged HealthChanged;
	FOnCombatantDefeated Defeated;
};
