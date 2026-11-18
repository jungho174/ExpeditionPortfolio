#include "UI/BattleCameraRig.h"
#include "Combat/CombatantVisualData.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "CollisionShape.h"
#include "GameFramework/PlayerController.h"

void ABattleCameraRig::ConfigureGroupShot_Implementation(const FBattleCameraShot& Shot,AActor* Player,const TArray<AActor*>& Targets)
{
	if (!IsValid(Player) || Targets.IsEmpty()) { return; }
	FBox Bounds(ForceInit);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleGroupCamera),false);
	Query.AddIgnoredActor(Player);Query.AddIgnoredActor(this);
	for (AActor* Target:Targets)
	{
		if (!IsValid(Target)) { continue; }
		FVector Center,Extent;Target->GetActorBounds(false,Center,Extent);
		Bounds+=FBox(Center-Extent,Center+Extent);
		Bounds+=UCombatantVisualData::ResolveAnchor(Target,ECombatantAnchor::HealthBar)+FVector(0,0,18);
		Query.AddIgnoredActor(Target);
	}
	if (!Bounds.IsValid) { return; }
	const FVector GroupCenter=Bounds.GetCenter();
	if (!Shot.bOriginAtEnemy)
	{
		// Non-colliding camera visualization components have very large editor bounds.
		FVector Center,Extent;Player->GetActorBounds(true,Center,Extent);
		Bounds+=FBox(Center-Extent,Center+Extent);
	}
	FVector Forward=(GroupCenter-Player->GetActorLocation()).GetSafeNormal2D();
	if (Forward.IsNearlyZero()) { Forward=Player->GetActorForwardVector(); }
	const FVector Right=FVector::CrossProduct(FVector::UpVector,Forward);
	const FVector Aim=Bounds.GetCenter()+FVector(0,0,Shot.LookHeight);
	float Aspect=16.f/9.f;
	if (const auto* PC=GetWorld()->GetFirstPlayerController())
	{
		int32 W=0,H=0;PC->GetViewportSize(W,H);if (W>0 && H>0) { Aspect=static_cast<float>(W)/H; }
	}
	const float TanHalf=FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Shot.FieldOfView,20.f,100.f)*.5f));
	const float Padding=FMath::Clamp(Shot.TargetFramingPadding,1.f,2.f);
	float Distance=FMath::Abs(Shot.Offset.X);
	for (int32 Corner=0;Corner<8;++Corner)
	{
		const FVector P((Corner&1)?Bounds.Max.X:Bounds.Min.X,(Corner&2)?Bounds.Max.Y:Bounds.Min.Y,(Corner&4)?Bounds.Max.Z:Bounds.Min.Z);
		const FVector Delta=P-Aim;
		const float Required=FMath::Max(FMath::Abs(FVector::DotProduct(Delta,Right)-Shot.Offset.Y),FMath::Abs(Delta.Z-Shot.Offset.Z)*Aspect)*Padding/TanHalf;
		Distance=FMath::Max(Distance,static_cast<float>(Required-FVector::DotProduct(Delta,Forward)));
	}
	FVector Position=Aim-Forward*Distance+Right*Shot.Offset.Y+FVector::UpVector*Shot.Offset.Z;
	if (Shot.bAvoidWorldObstacles)
	{
		FHitResult Hit;
		if (GetWorld()->SweepSingleByChannel(Hit,Aim,Position,FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(FMath::Clamp(Shot.CollisionRadius,1.f,30.f)),Query)) { Position=Hit.Location+Hit.Normal*2; }
	}
	SetActorLocationAndRotation(Position,(Aim-Position).Rotation());
	GetCameraComponent()->SetFieldOfView(FMath::Clamp(Shot.FieldOfView,20.f,100.f));
	GetCameraComponent()->SetConstraintAspectRatio(false);
}

void ABattleCameraRig::ConfigureShot_Implementation(const FBattleCameraShot& Shot, AActor* Player, AActor* Enemy)
{
	if (!IsValid(Player) || !IsValid(Enemy)) { return; }
	FVector Forward = (Enemy->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
	if (Forward.IsNearlyZero()) { Forward = Player->GetActorForwardVector(); }
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	FVector Position = (Shot.bOriginAtEnemy ? Enemy->GetActorLocation() : Player->GetActorLocation()) + Forward * Shot.Offset.X + Right * Shot.Offset.Y + FVector::UpVector * Shot.Offset.Z;
	FVector Aim = FMath::Lerp(UCombatantVisualData::ResolveAnchor(Player,ECombatantAnchor::Camera), UCombatantVisualData::ResolveAnchor(Enemy,ECombatantAnchor::Camera), FMath::Clamp(Shot.LookAtEnemyWeight, 0.0f, 1.0f))
		+ FVector::UpVector * Shot.LookHeight;
	if (Shot.bOriginAtEnemy && Shot.bFitTargetBounds)
	{
		FVector Center,Extent; Enemy->GetActorBounds(false,Center,Extent);
		const float Top=FMath::Max(Center.Z+Extent.Z,UCombatantVisualData::ResolveAnchor(Enemy,ECombatantAnchor::HealthBar).Z+12.f);
		const float Bottom=Center.Z-Extent.Z;
		float Aspect=16.f/9.f;
		if (GetWorld() && GetWorld()->GetFirstPlayerController())
		{
			int32 Width=0,Height=0;GetWorld()->GetFirstPlayerController()->GetViewportSize(Width,Height);
			if (Width>0 && Height>0) { Aspect=static_cast<float>(Width)/Height; }
		}
		const float Radius=FMath::Max((Top-Bottom)*.5f*Aspect,FMath::Max(Extent.X,Extent.Y));
		const float Distance=Radius/FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Shot.FieldOfView,20.f,100.f)*.5f))*FMath::Clamp(Shot.TargetFramingPadding,1.f,2.f);
		Aim=FVector(Center.X,Center.Y,(Top+Bottom)*.5f);
		Position=Aim-Forward*FMath::Max(FMath::Abs(Shot.Offset.X),Distance)+Right*Shot.Offset.Y+FVector::UpVector*Shot.Offset.Z*.25f;
	}
	if (Shot.bAvoidWorldObstacles && GetWorld())
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleCamera), false);
		Query.AddIgnoredActor(Player); Query.AddIgnoredActor(Enemy); Query.AddIgnoredActor(this);
		FHitResult Hit;
		if (GetWorld()->SweepSingleByChannel(Hit, Aim, Position, FQuat::Identity, ECC_Camera,
			FCollisionShape::MakeSphere(FMath::Clamp(Shot.CollisionRadius, 1.0f, 30.0f)), Query))
		{
			Position = Hit.Location + Hit.Normal * 2;
		}
	}
	SetActorLocationAndRotation(Position, (Aim - Position).Rotation());
	GetCameraComponent()->SetFieldOfView(FMath::Clamp(Shot.FieldOfView, 20.0f, 100.0f));
	GetCameraComponent()->SetConstraintAspectRatio(false);
}
