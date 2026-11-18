// Copyright Epic Games, Inc. All Rights Reserved.

#include "Exploration/Encounters/BattleEncounterTrigger.h"

#include "Combat/BattleCoordinator.h"
#include "Exploration/Encounters/EncounterProgressSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

ABattleEncounterTrigger::ABattleEncounterTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	EncounterVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("EncounterVolume"));
	SetRootComponent(EncounterVolume);
	EncounterVolume->InitBoxExtent(FVector(100.0, 100.0, 100.0));
	EncounterVolume->SetCollisionProfileName(TEXT("Trigger"));
	EncounterVolume->SetGenerateOverlapEvents(true);
}

void ABattleEncounterTrigger::BeginPlay()
{
	Super::BeginPlay();
	if (IsEncounterCleared())
	{
		const auto ClearedEnemies=GetEncounterEnemies();
		CompleteEncounter();
		for (AActor* ClearedEnemy:ClearedEnemies)
		{
			if (!IsValid(ClearedEnemy)) { continue; }
			ClearedEnemy->SetActorEnableCollision(false);
			ClearedEnemy->Destroy();
		}
		UE_LOG(LogBattleCoordinator,Log,TEXT("Cleared encounter restored: Map=%s Id=%s"),*GetEncounterMapId().ToString(),*EncounterId.ToString());
		return;
	}
	if (!EncounterId.IsValid())
	{
		UE_LOG(LogBattleCoordinator,Warning,TEXT("Encounter %s has no persistent ID. Run Tools/SetupEncounterPersistence.py and save the level."),*GetName());
	}

	EncounterVolume->OnComponentBeginOverlap.AddDynamic(
		this,
		&ABattleEncounterTrigger::HandleEncounterBeginOverlap);
}

void ABattleEncounterTrigger::HandleEncounterBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComponent;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (bEncounterTriggered || IsEncounterCleared())
	{
		return;
	}

	APawn* OverlappingPawn = Cast<APawn>(OtherActor);
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (OverlappingPawn == nullptr || PlayerController == nullptr || PlayerController->GetPawn() != OverlappingPawn)
	{
		return;
	}

	// Capture the approach before battle setup turns the pawn toward its opponent.
	FVector Approach=OverlappingPawn->GetVelocity().GetSafeNormal2D();
	if (Approach.IsNearlyZero()) { Approach=(GetActorLocation()-OverlappingPawn->GetActorLocation()).GetSafeNormal2D(); }
	if (Approach.IsNearlyZero()) { Approach=OverlappingPawn->GetActorForwardVector().GetSafeNormal2D(); }
	UBattleCoordinator* BattleCoordinator = GetWorld()->GetSubsystem<UBattleCoordinator>();
	TArray<APawn*> Party;
	for (APawn* Ally:AdditionalAllies) { Party.Add(Ally); }
	if (BattleCoordinator == nullptr || !BattleCoordinator->RequestGroupBattleStart(this, OverlappingPawn, GetEncounterEnemies(), Party))
	{
		return;
	}

	bEncounterTriggered = true;
	EntryBackDirection=-Approach;
	EncounterVolume->SetGenerateOverlapEvents(false);
	EncounterVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool ABattleEncounterTrigger::FindRetreatLocation(APawn* Player,FVector& OutLocation) const
{
	if (!Player || !GetWorld()) { return false; }
	const auto* Character=Cast<ACharacter>(Player);
	const float Radius=Character?Character->GetCapsuleComponent()->GetScaledCapsuleRadius():35.f;
	const float HalfHeight=Character?Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight():90.f;
	const FTransform VolumeTransform=EncounterVolume->GetComponentTransform();
	const FVector Extent=EncounterVolume->GetUnscaledBoxExtent();
	const FVector LocalDirection=VolumeTransform.InverseTransformVectorNoScale(EntryBackDirection).GetSafeNormal2D();
	const FVector Scale=VolumeTransform.GetScale3D().GetAbs();
	const float XDistance=FMath::Abs(LocalDirection.X)>.001f?Extent.X*Scale.X/FMath::Abs(LocalDirection.X):BIG_NUMBER;
	const float YDistance=FMath::Abs(LocalDirection.Y)>.001f?Extent.Y*Scale.Y/FMath::Abs(LocalDirection.Y):BIG_NUMBER;
	const FVector Boundary=VolumeTransform.GetLocation()+EntryBackDirection*(FMath::Min(XDistance,YDistance)+Radius+FMath::Max(20.f,RetreatDistance));
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BattleRetreat),false,Player);
	Query.AddIgnoredActor(this);
	for (int32 Attempt=0;Attempt<(RetreatPoint?1:8);++Attempt)
	{
		FVector Candidate=RetreatPoint?RetreatPoint->GetActorLocation():Boundary+EntryBackDirection*(Attempt*100.f);
		Candidate.Z=RetreatPoint?Candidate.Z:Player->GetActorLocation().Z;
		FHitResult Ground;
		if (!GetWorld()->LineTraceSingleByChannel(Ground,Candidate+FVector(0,0,250),Candidate-FVector(0,0,650),ECC_Visibility,Query)
			|| Ground.ImpactNormal.Z<.7f) { continue; }
		Candidate=Ground.ImpactPoint+FVector(0,0,HalfHeight+3.f);
		// Require the entire capsule to be outside even if a fixed point was supplied.
		const FVector Local=VolumeTransform.InverseTransformPosition(Candidate);
		const float LocalRadius=Radius/FMath::Max(.01f,FMath::Min(Scale.X,Scale.Y));
		if (FMath::Abs(Local.X)<=Extent.X+LocalRadius && FMath::Abs(Local.Y)<=Extent.Y+LocalRadius) { continue; }
		if (GetWorld()->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,HalfHeight),Query)) { continue; }
		OutLocation=Candidate;
		return true;
	}
	return false;
}

void ABattleEncounterTrigger::RearmAfterRetreat()
{
	if (IsEncounterCleared()) { return; }
	bEncounterTriggered=false;
	EncounterVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EncounterVolume->SetGenerateOverlapEvents(true);
}

void ABattleEncounterTrigger::EnsureEncounterId()
{
	if (IsTemplate() || EncounterId.IsValid()) { return; }
#if WITH_EDITOR
	if (GetWorld() && GetWorld()->WorldType==EWorldType::Editor) { Modify(); }
#endif
	EncounterId=FGuid::NewGuid();
}

void ABattleEncounterTrigger::PostActorCreated()
{
	Super::PostActorCreated();
	EnsureEncounterId();
}

#if WITH_EDITOR
void ABattleEncounterTrigger::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	if (DuplicateMode!=EDuplicateMode::PIE && !IsTemplate()) { EncounterId=FGuid::NewGuid(); }
}

void ABattleEncounterTrigger::PostEditImport()
{
	Super::PostEditImport();
	if (!IsTemplate()) { EncounterId=FGuid::NewGuid(); }
}
#endif

FName ABattleEncounterTrigger::GetEncounterMapId() const
{
	const ULevel* Level=GetLevel();
	return Level?FName(UWorld::RemovePIEPrefix(Level->GetOutermost()->GetName())):NAME_None;
}

bool ABattleEncounterTrigger::IsEncounterCleared() const
{
	if (bEncounterCleared) { return true; }
	const UGameInstance* Instance=GetWorld()?GetWorld()->GetGameInstance():nullptr;
	const auto* Progress=Instance?Instance->GetSubsystem<UEncounterProgressSubsystem>():nullptr;
	return Progress && Progress->IsCleared(GetEncounterMapId(),EncounterId);
}

void ABattleEncounterTrigger::CompleteEncounter()
{
	bEncounterCleared=true;
	bEncounterTriggered=true;
	EncounterVolume->SetGenerateOverlapEvents(false);
	EncounterVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UGameInstance* Instance=GetWorld()?GetWorld()->GetGameInstance():nullptr)
	{
		if (auto* Progress=Instance->GetSubsystem<UEncounterProgressSubsystem>()) { Progress->MarkCleared(GetEncounterMapId(),EncounterId); }
	}
	EnemyActor=nullptr;
}

TArray<AActor*> ABattleEncounterTrigger::GetEncounterEnemies() const
{
	return IsValid(EnemyActor) ? TArray<AActor*>{EnemyActor.Get()} : TArray<AActor*>{};
}
