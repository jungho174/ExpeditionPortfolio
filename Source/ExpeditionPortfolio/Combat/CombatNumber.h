#pragma once
#include "CoreMinimal.h"
#include "CombatNumber.generated.h"

UENUM(BlueprintType)
enum class ECombatNumberKind : uint8 { Damage, Healing, BonusDamage };

// An applied combat effect, never a command preview or an HP snapshot restoration.
USTRUCT(BlueprintType)
struct EXPEDITIONPORTFOLIO_API FCombatNumber
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Target = nullptr;
	UPROPERTY(BlueprintReadOnly) float Amount = 0;
	UPROPERTY(BlueprintReadOnly) ECombatNumberKind Kind = ECombatNumberKind::Damage;
};
