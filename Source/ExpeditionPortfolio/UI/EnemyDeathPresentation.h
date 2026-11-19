#pragma once

#include "CoreMinimal.h"
#include "EnemyDeathPresentation.generated.h"

class UEnemyPresentationData;
class USkeletalMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UAnimMontage;

USTRUCT()
struct FEnemyDeathPresentation
{
	GENERATED_BODY()

	bool IsActive() const { return bActive; }
	bool IsFading() const { return bFading; }
	void Capture(AActor& Actor);
	float PlayDeathPose(AActor& Actor, USkeletalMeshComponent* Mesh, const UEnemyPresentationData* Data);
	bool BeginFade(USkeletalMeshComponent* Mesh, const UEnemyPresentationData* Data);
	bool AdvanceFade(float DeltaTime);
	void Hide(AActor& Actor);
	void Restore(AActor* Actor, USkeletalMeshComponent* Mesh);

private:
	void RestoreMaterials(USkeletalMeshComponent* Mesh);
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> FadeInstances;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> Montage;
	FName FadeParameter;
	float Elapsed = 0.0f;
	float Duration = 0.0f;
	bool bActive = false;
	bool bFading = false;
	bool bWasHidden = false;
	bool bHadCollision = false;
};
