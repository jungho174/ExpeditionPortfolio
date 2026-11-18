#include "Combat/BattleEnemyActor.h"
#include "Combat/CombatantComponent.h"
#include "UI/EnemyAttackPresentationComponent.h"
ABattleEnemyActor::ABattleEnemyActor()
{
	PrimaryActorTick.bCanEverTick=false;
	Combatant=CreateDefaultSubobject<UCombatantComponent>(TEXT("Combatant"));
	EnemyPresentation=CreateDefaultSubobject<UEnemyAttackPresentationComponent>(TEXT("EnemyPresentation"));
}
