#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BattleEnemyActor.generated.h"
class UCombatantComponent;
class UEnemyAttackPresentationComponent;

// Shared enemy behavior. Species Blueprints supply their own mesh, collision, stats and content.
UCLASS(Blueprintable)
class EXPEDITIONPORTFOLIO_API ABattleEnemyActor : public ACharacter
{
	GENERATED_BODY()
public:
	ABattleEnemyActor();
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Combat") TObjectPtr<UCombatantComponent> Combatant;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Combat") TObjectPtr<UEnemyAttackPresentationComponent> EnemyPresentation;
};
