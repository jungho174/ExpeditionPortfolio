#pragma once

#include "CoreMinimal.h"
#include "Combat/CharacterActionData.h"
#include "Combat/ActionQTEPatternData.h"
#include "MagicActionData.generated.h"

class UAnimMontage;
class UNiagaraSystem;
class USoundBase;
class UCameraShakeBase;

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UMagicActionData : public UCharacterActionData
{
	GENERATED_BODY()
public:
	// Played once at release, after preparation (never when selecting the command).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Magic|Audio")
	TObjectPtr<USoundBase> AttackSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Magic|Audio", meta=(ClampMin="0.0", ClampMax="2.0"))
	float AttackSoundVolume = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Magic|Audio", meta=(ClampMin="0",ClampMax="5",Units="s")) float AttackSoundDelay = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Magic|Audio", meta=(ClampMin="0",Units="s")) float AttackSoundStartTime = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Magic|Audio") TObjectPtr<USoundBase> ImpactSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Magic|Audio", meta=(ClampMin="0",ClampMax="2")) float ImpactSoundVolume = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Magic|Audio", meta=(ClampMin="0",ClampMax="5",Units="s")) float ImpactSoundDelay = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Magic|Audio", meta=(ClampMin="0",Units="s")) float ImpactSoundStartTime = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic")
	TObjectPtr<UAnimMontage> ChargeMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Magic|Timing", meta=(ClampMin="0",Units="s")) float CastQTEHoldTime = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic")
	TObjectPtr<UAnimMontage> AttackMontage;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic", meta = (ClampMin = "0.01"))
	float AttackPlayRate = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic", meta = (ClampMin = "0.0", Units = "s"))
	float AttackStartTime = 0.0f;
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	TObjectPtr<UAnimMontage> CounterHitReaction;
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	float ParrySlowDuration = 0.15f;
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	float ParrySlowFactor = 0.15f;
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	TObjectPtr<UNiagaraSystem> ParryImpactEffect;
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	FVector ParryImpactScale = FVector(0.35f);
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	TObjectPtr<USoundBase> ParryImpactSound;
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	float ParrySoundVolume = 0.5f;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") float ParrySoundDelay = 0;
	UPROPERTY(BlueprintReadOnly,Category="Legacy") float ParrySoundStartTime = 0;
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	TSubclassOf<UCameraShakeBase> ParryCameraShake;
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	float ParryCameraShakeScale = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic")
	TObjectPtr<UNiagaraSystem> ChargeEffect;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic")
	TObjectPtr<UNiagaraSystem> AttackEffect;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic")
	FName CastSocket = TEXT("hand_r");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic")
	FTransform ChargeEffectTransform;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic")
	FTransform AttackEffectTransform;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic")
	bool bAttackEffectAtTarget = false;
	// Directional spells start at CastSocket and aim at the target, independently of either actor's yaw.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic", meta = (EditCondition = "!bAttackEffectAtTarget"))
	bool bAttackEffectAimAtTarget = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic")
	bool bAttackEffectOnHit = false;
	// The montage marker launches this spell; damage is reported only after arrival.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic|Projectile")
	bool bUseProjectile = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic|Projectile", meta = (EditCondition = "bUseProjectile", ClampMin = "1.0"))
	float ProjectileSpeed = 1500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic|Projectile", meta = (EditCondition = "bUseProjectile", ClampMin = "0.05", ClampMax = "10.0"))
	float MaxFlightDuration = 3.0f;
	UPROPERTY(BlueprintReadOnly,Category="Legacy")
	FName TargetSocket = TEXT("spine_03");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic|Projectile", meta = (EditCondition = "bUseProjectile"))
	TObjectPtr<UNiagaraSystem> ImpactEffect;
	// Relative to the arrival point, facing from caster to target. Not the enemy actor's yaw.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic|Projectile", meta = (EditCondition = "bUseProjectile"))
	FTransform ImpactEffectTransform;

	bool ValidateProjectile() const
	{
		return !bUseProjectile || (FMath::IsFinite(ProjectileSpeed) && ProjectileSpeed > 0
			&& FMath::IsFinite(MaxFlightDuration) && MaxFlightDuration >= 0.05f && MaxFlightDuration <= 10);
	}
	float GetFlightDuration(float Distance) const
	{
		return bUseProjectile && ValidateProjectile() && FMath::IsFinite(Distance) && Distance >= 0
			? FMath::Clamp(Distance / ProjectileSpeed, 0.05f, MaxFlightDuration) : 0;
	}
	// Extra playback after a normally completed montage, followed by natural particle completion.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float AttackEffectTailDuration = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic")
	bool bChargeEffectBetweenHands = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic")
	FName LeftHandSocket = TEXT("hand_l");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magic")
	FName RightHandSocket = TEXT("hand_r");
};
