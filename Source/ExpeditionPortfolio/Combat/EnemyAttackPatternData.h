#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemyAttackPatternData.generated.h"

class UAnimMontage;
class UNiagaraSystem;
class USoundBase;

USTRUCT(BlueprintType)
struct EXPEDITIONPORTFOLIO_API FEnemyAttackHit
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit", meta=(ClampMin="0.001")) float Damage = 10;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit", meta=(ClampMin="0.001",Units="s")) float DefenseWindowDuration = .45f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit", meta=(ClampMin="0.001",Units="s")) float ParrySuccessDuration = .2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit", meta=(Units="s")) float HitTimeOffset = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit", meta=(ClampMin="0.001",Units="s")) float FallbackHitTime = 1;
	// Resolved from the corresponding indexed EnemyHit marker when the pattern starts.
	UPROPERTY(BlueprintReadOnly, Transient, Category="Runtime") float HitTime = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") TObjectPtr<USoundBase> ImpactSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback",meta=(ClampMin="0")) float SoundVolume = .65f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback",meta=(ClampMin="0",Units="s")) float SoundDelay = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback",meta=(ClampMin="0",Units="s")) float SoundStartTime = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback") TObjectPtr<UNiagaraSystem> ImpactEffect;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Feedback",meta=(ClampMin="0")) float EffectScale = .5f;
};

// Copied at selection time so editing an asset cannot change an attack in flight.
USTRUCT(BlueprintType)
struct EXPEDITIONPORTFOLIO_API FEnemyAttackPatternSnapshot
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Pattern") TArray<FEnemyAttackHit> Hits;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Pattern")
	FName PatternId = TEXT("BasicAttack");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Pattern")
	FText DisplayName = FText::FromString(TEXT("Basic Attack"));

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Pattern")
	float Damage = 20.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Pattern")
	float TelegraphDuration = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Pattern")
	float DefenseWindowDuration = 0.6f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Pattern")
	float ParrySuccessDuration = 0.2f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Pattern")
	TObjectPtr<UAnimMontage> AttackMontage;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Pattern")
	float HitTime = 0.6f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Pattern")
	float HitTimeOffset = 0.0f;
};

UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UEnemyAttackPatternData : public UDataAsset
{
	GENERATED_BODY()

public:
	// Empty preserves legacy single-hit tuning. An entry represents one separately defendable hit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hits",meta=(TitleProperty="Damage")) TArray<FEnemyAttackHit> Hits;
	TArray<FEnemyAttackHit> ResolveHits() const;
	UFUNCTION(BlueprintPure,Category="Pattern") FString GetValidationError() const { FString Reason; ValidatePattern(Reason); return Reason; }
	UFUNCTION(BlueprintPure,Category="Pattern") TArray<FEnemyAttackHit> GetResolvedHits() const { return ResolveHits(); }
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	FName PatternId = TEXT("BasicAttack");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern")
	FText DisplayName = FText::FromString(TEXT("Basic Attack"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Legacy Single Hit", meta = (ClampMin = "0.001"))
	float Damage = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern", meta = (ClampMin = "0.001", Units = "s"))
	float TelegraphDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Legacy Single Hit", meta = (ClampMin = "0.001", Units = "s"))
	float DefenseWindowDuration = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Legacy Single Hit", meta = (ClampMin = "0.001", Units = "s"))
	float ParrySuccessDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pattern", meta = (ClampMin = "0.0"))
	float SelectionWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> AttackMontage;
	// Negative = earlier, positive = later. Defense/parry windows move with this hit time.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Legacy Single Hit", meta = (Units = "s"))
	float HitTimeOffset = 0.0f;
	// Used only when the montage's EnemyHit notify is missing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Legacy Single Hit", meta = (ClampMin = "0.001", Units = "s"))
	float FallbackHitTime = 4.7f;

	float GetHitTime() const;

	bool ValidatePattern(FString& OutReason) const;
	FEnemyAttackPatternSnapshot MakeSnapshot() const;

	// 이번 턴의 유효한 후보만 추첨하고 선택한 타격 정보를 복사한다.
	static const UEnemyAttackPatternData* SelectPattern(
		const TArray<TObjectPtr<UEnemyAttackPatternData>>& Patterns, FRandomStream& Random,
		FEnemyAttackPatternSnapshot* OutSnapshot = nullptr,
		const TFunction<void(const UEnemyAttackPatternData*, const FString&)>& OnInvalid = {});

private:
	TArray<FEnemyAttackHit> ResolveHits(TArray<int32>& OutMarkerCounts) const;
	bool TryResolveValidHits(TArray<FEnemyAttackHit>& OutHits, FString& OutReason) const;
	FEnemyAttackPatternSnapshot MakeSnapshot(TArray<FEnemyAttackHit> ResolvedHits) const;
};
