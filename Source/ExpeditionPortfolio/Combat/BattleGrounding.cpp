#include "Combat/BattleGrounding.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

namespace
{
	float Safe(float Value,float Default,float Min,float Max) { return FMath::IsFinite(Value)?FMath::Clamp(Value,Min,Max):Default; }
	FBattleGroundSupport Support(UWorld* World,FVector Candidate,FVector Capsule,const FBattleGroundingSettings& Settings,const FCollisionQueryParams& Query)
	{
		FBattleGroundSupport Result;Result.Location=Candidate;
		if (!World || Candidate.ContainsNaN()) { return Result; }
		const float Radius=Safe(Capsule.X,40,1,500),HalfHeight=FMath::Max(Radius,Safe(Capsule.Z,90,1,1000));
		FHitResult Hit;
		const FVector Start=Candidate+FVector(0,0,Safe(Settings.MaxStepHeight,60,1,500)+2);
		const FVector End=Candidate-FVector(0,0,Safe(Settings.MaxDropHeight,150,1,1000));
		if (World->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,HalfHeight),Query)
			&& !Hit.bStartPenetrating && Hit.ImpactNormal.Z>=FMath::Cos(FMath::DegreesToRadians(Safe(Settings.MaxSlopeAngle,50,0,75))))
		{
			Result.bSupported=true;Result.Location=Hit.Location+FVector(0,0,2);
		}
		return Result;
	}
}

FVector UBattleGroundingLibrary::GetCapsuleExtent(const AActor* Actor)
{
	if (Actor)
	{
		if (const auto* Capsule=Actor->FindComponentByClass<UCapsuleComponent>())
		{ return FVector(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()); }
	}
	return FVector(40,40,90);
}

FBattleGroundSupport UBattleGroundingLibrary::QueryActorGround(AActor* Actor,FVector Candidate,const FBattleGroundingSettings& Settings)
{
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleGrounding),true,Actor);
	if (Actor) { Query.AddIgnoredActor(Actor->GetAttachParentActor()); }
	return Support(Actor?Actor->GetWorld():nullptr,Candidate,GetCapsuleExtent(Actor),Settings,Query);
}

bool UBattleGroundingLibrary::BuildPath(UWorld* World,FVector Start,FVector Goal,FVector Capsule,const FBattleGroundingSettings& Settings,
	const FCollisionQueryParams& Query,TArray<FVector>& Points,FString& Failure,const bool bAllowPartial)
{
	Points.Reset();Failure.Reset();
	if (!World || Start.ContainsNaN() || Goal.ContainsNaN()) { Failure=TEXT("invalid movement endpoints");return false; }
	if (!Settings.bEnabled) { Points={Start,Goal};return true; }
	const int32 Count=FMath::Clamp(FMath::CeilToInt(FVector::Dist2D(Start,Goal)/Safe(Settings.SampleSpacing,25,5,100)),1,256);
	const float Radius=Safe(Capsule.X,40,1,500),HalfHeight=FMath::Max(Radius,Safe(Capsule.Z,90,1,1000));
	const FCollisionShape Body=FCollisionShape::MakeCapsule(FMath::Max(1.f,Radius-1),FMath::Max(Radius,HalfHeight-1));
	for (int32 Index=0;Index<=Count;++Index)
	{
		FVector Candidate=FMath::Lerp(Start,Goal,static_cast<float>(Index)/Count);
		Candidate.Z=Points.IsEmpty()?Start.Z:Points.Last().Z;
		const auto Ground=Support(World,Candidate,Capsule,Settings,Query);
		if (!Ground.bSupported)
		{
			Failure=TEXT("ground missing, too steep, or outside step/drop range");
			if (bAllowPartial && !Points.IsEmpty()) { return true; }
			Points.Reset();return false;
		}
		if (!Points.IsEmpty())
		{
			const FVector Before=Points.Last(),After=Ground.Location;
			const float High=FMath::Max(Before.Z,After.Z);
			const FVector RaisedBefore(Before.X,Before.Y,High),RaisedAfter(After.X,After.Y,High);
			if (World->SweepTestByChannel(Before,RaisedBefore,FQuat::Identity,ECC_Pawn,Body,Query)
				|| World->SweepTestByChannel(RaisedBefore,RaisedAfter,FQuat::Identity,ECC_Pawn,Body,Query)
				|| World->SweepTestByChannel(RaisedAfter,After,FQuat::Identity,ECC_Pawn,Body,Query))
			{
				Failure=TEXT("body clearance blocked");
				if (bAllowPartial) { return true; }
				Points.Reset();return false;
			}
		}
		Points.Add(Ground.Location);
	}
	return true;
}

FVector UBattleGroundingLibrary::EvaluatePath(const TArray<FVector>& Points,float Alpha)
{
	if (Points.IsEmpty()) { return FVector::ZeroVector; }
	const float Sample=FMath::Clamp(Alpha,0.f,1.f)*(Points.Num()-1);
	const int32 Index=FMath::FloorToInt(Sample);
	return Points.IsValidIndex(Index+1)?FMath::Lerp(Points[Index],Points[Index+1],Sample-Index):Points.Last();
}
