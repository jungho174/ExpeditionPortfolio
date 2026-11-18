#include "Combat/BattleFacing.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void FBattleFacingRestore::Capture(AActor* Actor)
{
	Character = Cast<ACharacter>(Actor);
	if (Character.IsValid())
	{
		auto* Movement = Character->GetCharacterMovement();
		bOrientToMovement = Movement->bOrientRotationToMovement;
		bControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
		bControllerYaw = Character->bUseControllerRotationYaw;
		Movement->StopMovementImmediately();
		Movement->bOrientRotationToMovement = false;
		Movement->bUseControllerDesiredRotation = false;
		Character->bUseControllerRotationYaw = false;
	}
}
void FBattleFacingRestore::Restore()
{
	if (Character.IsValid())
	{
		auto* Movement = Character->GetCharacterMovement();
		Movement->bOrientRotationToMovement = bOrientToMovement;
		Movement->bUseControllerDesiredRotation = bControllerDesiredRotation;
		Character->bUseControllerRotationYaw = bControllerYaw;
	}
	Character.Reset();
}
void FBattleFacingRestore::Face(AActor* Actor, const AActor* Target)
{
	if (!IsValid(Actor) || !IsValid(Target)) { return; }
	const FVector Direction = (Target->GetActorLocation() - Actor->GetActorLocation()).GetSafeNormal2D();
	if (!Direction.IsNearlyZero()) { Actor->SetActorRotation(Direction.Rotation()); }
}
