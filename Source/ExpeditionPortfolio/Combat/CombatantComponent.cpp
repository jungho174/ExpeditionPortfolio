// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/CombatantComponent.h"

UCombatantComponent::UCombatantComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatantComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxHealth = FMath::Max(MaxHealth, 1.0f);
	CurrentHealth = MaxHealth;
}

bool UCombatantComponent::ApplyDamage(const float DamageAmount)
{
	if (!FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f || IsDefeated())
	{
		return false;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);

	if (CurrentHealth == PreviousHealth)
	{
		return false;
	}

	HealthChanged.Broadcast(PreviousHealth, CurrentHealth);

	if (IsDefeated())
	{
		Defeated.Broadcast();
	}

	return true;
}
