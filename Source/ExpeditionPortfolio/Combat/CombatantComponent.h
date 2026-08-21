// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "CombatantComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnCombatantHealthChanged,
	float /* PreviousHealth */,
	float /* NewHealth */);

DECLARE_MULTICAST_DELEGATE(FOnCombatantDefeated);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class EXPEDITIONPORTFOLIO_API UCombatantComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatantComponent();

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Combat|Health")
	bool IsDefeated() const { return CurrentHealth <= 0.0f; }

	UFUNCTION(BlueprintCallable, Category = "Combat|Health")
	bool ApplyDamage(float DamageAmount);

	FOnCombatantHealthChanged& OnHealthChanged() { return HealthChanged; }
	FOnCombatantDefeated& OnDefeated() { return Defeated; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditAnywhere, Category = "Combat|Health", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Combat|Health")
	float CurrentHealth = 100.0f;

	FOnCombatantHealthChanged HealthChanged;
	FOnCombatantDefeated Defeated;
};
