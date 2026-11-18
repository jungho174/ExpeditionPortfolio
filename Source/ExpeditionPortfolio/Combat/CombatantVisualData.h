#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatantVisualData.generated.h"

UENUM(BlueprintType,meta=(ScriptName="CombatantAnchorKind"))
enum class ECombatantAnchor : uint8 { Aim, DamageNumber, TargetMarker, ParryIntermediate, ParryComplete, Camera, HealthBar };

USTRUCT(BlueprintType)
struct FCombatantAnchor
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Socket;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector WorldOffset=FVector::ZeroVector;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bBoundsTopFallback=false;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(EditCondition="bBoundsTopFallback")) float BoundsTopOffset=0;
};

// Per-character/monster anatomy. Shared effects and widgets consume semantic anchors only.
UCLASS(BlueprintType)
class EXPEDITIONPORTFOLIO_API UCombatantVisualData : public UDataAsset
{
	GENERATED_BODY()
public:
	UCombatantVisualData();
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Anchors") FCombatantAnchor HealthBar;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="HUD") TObjectPtr<class UTexture2D> Portrait;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Anchors") FCombatantAnchor Aim;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Anchors") FCombatantAnchor DamageNumber;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Anchors") FCombatantAnchor TargetMarker;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Anchors") FCombatantAnchor ParryIntermediate;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Anchors") FCombatantAnchor ParryComplete;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Anchors") FCombatantAnchor Camera;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Menu") FName MenuMotionBone;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Menu") FName MenuReferenceBone;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Menu") FVector MenuWorldOffset=FVector::ZeroVector;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Menu") FVector2D MenuScreenOffset=FVector2D::ZeroVector;
	const FCombatantAnchor& GetAnchor(ECombatantAnchor Kind) const;
	static UCombatantVisualData* Find(const AActor* Actor);
	static FVector ResolveAnchor(const AActor* Actor,ECombatantAnchor Kind);
};
