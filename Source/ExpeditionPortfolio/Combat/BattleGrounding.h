#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BattleGrounding.generated.h"
class AActor;
class UWorld;
struct FCollisionQueryParams;

USTRUCT(BlueprintType)
struct FBattleGroundingSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere,BlueprintReadOnly) bool bEnabled=true;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1",Units="cm")) float MaxStepHeight=60;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1",Units="cm")) float MaxDropHeight=150;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="5",Units="cm")) float SampleSpacing=25;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="0",ClampMax="75",Units="deg")) float MaxSlopeAngle=50;
};

USTRUCT(BlueprintType)
struct FBattleGroundSupport
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bSupported=false;
	UPROPERTY(BlueprintReadOnly) FVector Location=FVector::ZeroVector;
};

// Shared ground/path queries. Content supplies formation, capsule dimensions and tolerances.
UCLASS()
class EXPEDITIONPORTFOLIO_API UBattleGroundingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable,Category="Combat|Grounding")
	static FBattleGroundSupport QueryActorGround(AActor* Actor,FVector Candidate,const FBattleGroundingSettings& Settings);
	static FVector GetCapsuleExtent(const AActor* Actor);
	// Optional partial routes stop at the last safe sample (e.g. a space-limited visual dodge).
	static bool BuildPath(UWorld* World,FVector Start,FVector Goal,FVector Capsule,const FBattleGroundingSettings& Settings,
		const FCollisionQueryParams& Query,TArray<FVector>& Points,FString& Failure,bool bAllowPartial=false);
	static FVector EvaluatePath(const TArray<FVector>& Points,float Alpha);
};
