// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/CombatantComponent.h"
#include "Combat/PartyHealthSubsystem.h"
#include "Combat/CharacterCombatData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UCombatantComponent::UCombatantComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatantComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxHealth = FMath::Max(MaxHealth, 1.0f);
	CurrentHealth = MaxHealth;
	if (UGameInstance* Instance = GetWorld()->GetGameInstance())
	{
  auto* Health=Instance->GetSubsystem<UPartyHealthSubsystem>();
  RegisteredCharacterId=GetCharacterId();
  bIdentityRegistered=Persistence==ECombatantPersistence::UniquePartyMember && Health->RegisterCharacter(RegisteredCharacterId,this);
  if (Persistence==ECombatantPersistence::UniquePartyMember && !bIdentityRegistered)
  { UE_LOG(LogTemp,Error,TEXT("Party identity is missing or duplicated on %s. Configure the character profile's unique ID."),*GetNameSafe(GetOwner())); }
  float SavedHealth;
  if (bIdentityRegistered && Health->FindHealth(RegisteredCharacterId, SavedHealth))
		{
			CurrentHealth = FMath::Clamp(SavedHealth, 0.f, MaxHealth);
		}
	}
	StorePersistentHealth();
}

void UCombatantComponent::StorePersistentHealth()
{
	if (bIdentityRegistered && GetWorld())
	{
		if (UGameInstance* Instance = GetWorld()->GetGameInstance())
		{
			Instance->GetSubsystem<UPartyHealthSubsystem>()->StoreHealth(RegisteredCharacterId, CurrentHealth);
		}
	}
}

bool UCombatantComponent::RestoreBattleHealth(float Health)
{
	if (!FMath::IsFinite(Health)) { return false; }
	const float Previous = CurrentHealth;
	CurrentHealth = FMath::Clamp(Health, 0.f, MaxHealth);
	StorePersistentHealth();
	if (Previous != CurrentHealth)
	{
		HealthChanged.Broadcast(Previous, CurrentHealth);
		HealthChangedBlueprint.Broadcast(Previous, CurrentHealth);
	}
	return true;
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

	StorePersistentHealth();
	HealthChanged.Broadcast(PreviousHealth, CurrentHealth);
	HealthChangedBlueprint.Broadcast(PreviousHealth, CurrentHealth);

	if (IsDefeated())
	{
		Defeated.Broadcast();
		DefeatedBlueprint.Broadcast();
	}

	return true;
}

bool UCombatantComponent::RestoreHealth(const float Amount)
{
	if (!FMath::IsFinite(Amount) || Amount <= 0.0f || IsDefeated() || CurrentHealth >= MaxHealth) { return false; }
	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + Amount);
	StorePersistentHealth();
	HealthChanged.Broadcast(PreviousHealth, CurrentHealth);
	HealthChangedBlueprint.Broadcast(PreviousHealth, CurrentHealth);
	return true;
}

FName UCombatantComponent::GetCharacterId() const
{
 return Persistence==ECombatantPersistence::UniquePartyMember && CharacterActions?CharacterActions->CharacterId:NAME_None;
}
void UCombatantComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 if (bIdentityRegistered && GetWorld() && GetWorld()->GetGameInstance())
 { GetWorld()->GetGameInstance()->GetSubsystem<UPartyHealthSubsystem>()->UnregisterCharacter(RegisteredCharacterId,this); }
 bIdentityRegistered=false;RegisteredCharacterId=NAME_None;
 Super::EndPlay(Reason);
}
