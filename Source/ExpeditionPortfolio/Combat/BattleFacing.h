#pragma once
#include "CoreMinimal.h"

class AActor;
class ACharacter;

struct FBattleFacingRestore
{
	TWeakObjectPtr<ACharacter> Character;
	bool bOrientToMovement = false;
	bool bControllerDesiredRotation = false;
	bool bControllerYaw = false;
	void Capture(AActor* Actor);
	void Restore();
	static void Face(AActor* Actor, const AActor* Target);
};
