#include "UI/EnemyAttackPresentationComponent.h"
#include "UI/EnemyPresentationData.h"
#include "UI/CombatSoundPlayback.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/BattleCoordinator.h"
#include "Combat/CombatantComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

UEnemyAttackPresentationComponent::UEnemyAttackPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick=true;
	PrimaryComponentTick.bStartWithTickEnabled=false;
	PrimaryComponentTick.TickGroup=TG_PostUpdateWork;
}
void UEnemyAttackPresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	Mesh=GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	Combatant=GetOwner()->FindComponentByClass<UCombatantComponent>();
	if (Combatant.IsValid()) { HealthHandle=Combatant->OnHealthChanged().AddUObject(this,&ThisClass::OnHealthChanged); }
	if (Combatant.IsValid() && Combatant->IsDefeated()) { BeginDeathPresentation(); }
}
float UEnemyAttackPresentationComponent::GetExpectedDuration(UAnimMontage* Montage) const
{
	return (Montage?Montage->GetPlayLength():0)+(PresentationData && PresentationData->bApproachTarget
		? FMath::Clamp(PresentationData->ApproachDuration,.05f,5.f)+FMath::Clamp(PresentationData->ReturnDuration,.05f,5.f):0);
}
bool UEnemyAttackPresentationComponent::BeginAttack(UAnimMontage* Montage,const int64 ActionId,AActor* Target)
{
	if (Death.IsActive() || (Combatant.IsValid() && Combatant->IsDefeated())) { return false; }
	StopPresentation();
	bTravelBlocked=false;
	Mesh=GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	if (!Montage || !Mesh.IsValid() || !Mesh->GetAnimInstance() || !IsValid(Target)) { return false; }
	FVector Facing=Target->GetActorLocation()-GetOwner()->GetActorLocation(); Facing.Z=0;
	if (!Facing.IsNearlyZero()) { GetOwner()->SetActorRotation(Facing.Rotation()); }
	ActiveId=ActionId; StrikeMontage=Montage;
	OriginalTransform=Mesh->GetRelativeTransform(); bSavedTransform=true;
	if (PresentationData && PresentationData->bApproachTarget)
	{
		const float Distance=FMath::IsFinite(PresentationData->TargetDistance)?FMath::Clamp(PresentationData->TargetDistance,10.f,1000.f):200;
		TravelOffset=GetOwner()->GetActorTransform().InverseTransformVector(Facing.GetSafeNormal()*FMath::Max(0.f,Facing.Size()-Distance));
		TravelHome=GetOwner()->GetActorLocation();TravelMeshHome=Mesh->GetComponentLocation();
		FCollisionQueryParams Query(SCENE_QUERY_STAT(EnemyGroundTravel),true,GetOwner());Query.AddIgnoredActor(Target);
		Query.AddIgnoredActor(GetOwner()->GetAttachParentActor());
		if (auto* Battle=GetWorld()->GetSubsystem<UBattleCoordinator>())
		{
			Query.AddIgnoredActors(Battle->GetBattleAllies());Query.AddIgnoredActors(Battle->GetBattleEnemies());
			Query.AddIgnoredActor(Battle->GetActiveEncounterSource());
		}
		FString Failure;
		const FVector Goal=TravelHome+GetOwner()->GetActorTransform().TransformVector(TravelOffset);
		if (!UBattleGroundingLibrary::BuildPath(GetWorld(),TravelHome,Goal,UBattleGroundingLibrary::GetCapsuleExtent(GetOwner()),PresentationData->Grounding,Query,GroundTravelPoints,Failure))
		{
			bTravelBlocked=true;StopPresentation();
			UE_LOG(LogTemp,Warning,TEXT("Enemy approach cancelled: Actor=%s Reason=%s"),*GetNameSafe(GetOwner()),*Failure);return false;
		}
		StartTravel(false); return true;
	}
	if (StartStrike()) { return true; }
	StopPresentation(); return false;
}
bool UEnemyAttackPresentationComponent::StartStrike()
{
	if (!Mesh.IsValid() || !Mesh->GetAnimInstance() || !StrikeMontage) { return false; }
	if (ActiveMontage) { Mesh->GetAnimInstance()->Montage_Stop(.08f,ActiveMontage); }
	ActiveMontage=StrikeMontage;
	if (Mesh->GetAnimInstance()->Montage_Play(ActiveMontage)<=0) { return false; }
	const auto* Instance=Mesh->GetAnimInstance()->GetActiveInstanceForMontage(ActiveMontage);
	ActiveInstanceId=Instance?Instance->GetInstanceID():INDEX_NONE;
	FOnMontageEnded Ended; Ended.BindUObject(this,&ThisClass::OnMontageEnded,ActiveId);
	Mesh->GetAnimInstance()->Montage_SetEndDelegate(Ended,ActiveMontage);
	FOnMontageBlendingOutStarted Blending; Blending.BindUObject(this,&ThisClass::OnMontageBlendingOut,ActiveId);
	Mesh->GetAnimInstance()->Montage_SetBlendingOutDelegate(Blending,ActiveMontage);
	GetWorld()->GetSubsystem<UBattleCoordinator>()->NotifyEnemyAttackStarted(ActiveId);
	return true;
}
void UEnemyAttackPresentationComponent::StartTravel(const bool bReturn)
{
	bTravel=true; bReturning=bReturn; TravelElapsed=0;
	const float Config=bReturn?PresentationData->ReturnDuration:PresentationData->ApproachDuration;
	TravelDuration=FMath::IsFinite(Config)?FMath::Clamp(Config,.05f,5.f):.6f;
	ActiveMontage=PresentationData->WalkMontage;
	if (ActiveMontage && Mesh.IsValid() && Mesh->GetAnimInstance())
	{ Mesh->GetAnimInstance()->Montage_Play(ActiveMontage,bReturn?-1.f:1.f,EMontagePlayReturnType::MontageLength,bReturn?ActiveMontage->GetPlayLength():0); }
	const auto& Sound=bReturn?PresentationData->ReturnSound:PresentationData->ApproachSound;
	CombatSoundPlayback::Play(GetOwner(),Sound.Sound,Mesh->GetComponentLocation(),Sound.Volume,Sound.Delay,Sound.StartTime);
	SetComponentTickEnabled(true);
}
void UEnemyAttackPresentationComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime,TickType,TickFunction);
	if (Death.IsFading()) { UpdateDeathFade(DeltaTime); return; }
	if (!ActiveId || !Mesh.IsValid() || !bTravel) { Finish(true); return; }
	TravelElapsed+=DeltaTime;
	const float Alpha=FMath::Clamp(TravelElapsed/TravelDuration,0.f,1.f);
	const float Smooth=Alpha*Alpha*(3-2*Alpha);
	const FVector Root=UBattleGroundingLibrary::EvaluatePath(GroundTravelPoints,bReturning?1-Smooth:Smooth);
	Mesh->SetWorldLocation(TravelMeshHome+(Root-TravelHome));
	if (Alpha>=1)
	{
		bTravel=false; SetComponentTickEnabled(false);
		if (bReturning) { Finish(false); }
		else if (!StartStrike()) { Finish(true); }
	}
}
void UEnemyAttackPresentationComponent::HandleHitNotify(USkeletalMeshComponent* SourceMesh,const int32 InstanceId,const int32 HitIndex)
{
	if (ActiveId && SourceMesh==Mesh.Get() && InstanceId==ActiveInstanceId)
	{ GetWorld()->GetSubsystem<UBattleCoordinator>()->NotifyEnemyActionHit(ActiveId,HitIndex); }
}
void UEnemyAttackPresentationComponent::OnMontageBlendingOut(UAnimMontage* Montage,const bool bInterrupted,const int64 ExpectedId)
{
	if (bInterrupted && ActiveId==ExpectedId && ActiveMontage==Montage) { Finish(true); }
}
void UEnemyAttackPresentationComponent::OnMontageEnded(UAnimMontage* Montage,const bool bInterrupted,const int64 ExpectedId)
{
	if (!ActiveId || ExpectedId!=ActiveId || Montage!=ActiveMontage) { return; }
	if (!bInterrupted && PresentationData && PresentationData->bApproachTarget) { StartTravel(true); }
	else { Finish(bInterrupted); }
}
void UEnemyAttackPresentationComponent::Finish(const bool bInterrupted)
{
	const int64 Id=ActiveId; StopPresentation();
	if (Id && GetWorld()) { GetWorld()->GetSubsystem<UBattleCoordinator>()->NotifyEnemyActionFinished(Id,bInterrupted); }
}
void UEnemyAttackPresentationComponent::StopPresentation()
{
	ActiveId=0; bTravel=false; bHeldForCounter=false; SetComponentTickEnabled(Death.IsFading());
	if (Mesh.IsValid() && Mesh->GetAnimInstance() && ActiveMontage)
	{
		FOnMontageEnded Empty; FOnMontageBlendingOutStarted EmptyBlend;
		Mesh->GetAnimInstance()->Montage_SetEndDelegate(Empty,ActiveMontage);
		Mesh->GetAnimInstance()->Montage_SetBlendingOutDelegate(EmptyBlend,ActiveMontage);
		Mesh->GetAnimInstance()->Montage_Stop(.1f,ActiveMontage);
	}
	if (!Death.IsActive())
	{
		if (bSavedTransform && Mesh.IsValid()) { Mesh->SetRelativeTransform(OriginalTransform); }
		bSavedTransform=false;
	}
	ActiveMontage=nullptr; StrikeMontage=nullptr; ActiveInstanceId=INDEX_NONE;
	GroundTravelPoints.Reset();
}
bool UEnemyAttackPresentationComponent::HasHitReaction() const { return PresentationData && PresentationData->HitReaction; }
void UEnemyAttackPresentationComponent::HoldForCounter()
{
	if (!ActiveId || !Mesh.IsValid() || !Mesh->GetAnimInstance()) { return; }
	bHeldForCounter=true; bTravel=false; SetComponentTickEnabled(false);
	if (ActiveMontage)
	{
		FOnMontageEnded Empty; FOnMontageBlendingOutStarted EmptyBlend;
		Mesh->GetAnimInstance()->Montage_SetEndDelegate(Empty,ActiveMontage);
		Mesh->GetAnimInstance()->Montage_SetBlendingOutDelegate(EmptyBlend,ActiveMontage);
		Mesh->GetAnimInstance()->Montage_Pause(ActiveMontage);
	}
}
bool UEnemyAttackPresentationComponent::ReturnAfterCounter()
{
	if (!bHeldForCounter) { return false; }
	bHeldForCounter=false;
	if (Mesh.IsValid() && Mesh->GetAnimInstance() && ActiveMontage)
	{
		FOnMontageEnded Empty; FOnMontageBlendingOutStarted EmptyBlend;
		Mesh->GetAnimInstance()->Montage_SetEndDelegate(Empty,ActiveMontage);
		Mesh->GetAnimInstance()->Montage_SetBlendingOutDelegate(EmptyBlend,ActiveMontage);
		Mesh->GetAnimInstance()->Montage_Stop(.1f,ActiveMontage);
	}
	if (PresentationData && PresentationData->bApproachTarget && ActiveId && Mesh.IsValid()) { StartTravel(true); return true; }
	StopPresentation(); return false;
}
void UEnemyAttackPresentationComponent::OnHealthChanged(float Before,float After)
{
	if (After>0 && Death.IsActive()) { ResetDeathPresentation(); }
	if (After>=Before) { return; }
	if (After<=0) { BeginDeathPresentation(); return; }
	if (!PresentationData || !Mesh.IsValid() || !Mesh->GetAnimInstance()) { return; }
	if (ActiveId && !bHeldForCounter) { return; }
	UAnimMontage* Reaction=PresentationData->HitReaction.Get();
	if (Reaction) { Mesh->GetAnimInstance()->Montage_Play(Reaction); }
	const auto& Sound=PresentationData->HitSound;
	CombatSoundPlayback::Play(GetOwner(),Sound.Sound,Mesh->GetComponentLocation(),Sound.Volume,Sound.Delay,Sound.StartTime);
}

void UEnemyAttackPresentationComponent::BeginDeathPresentation()
{
	if (Death.IsActive() || !GetOwner() || !GetWorld()) { return; }
	Death.Capture(*GetOwner());
	if (Mesh.IsValid() && !bSavedTransform) { OriginalTransform = Mesh->GetRelativeTransform(); bSavedTransform = true; }
	// 공격 콜백을 끊되 적이 쓰러진 위치는 복원 시점까지 유지한다.
	StopPresentation();
	const float Lifetime = Death.PlayDeathPose(*GetOwner(), Mesh.Get(), PresentationData);
	GetWorld()->GetTimerManager().SetTimer(DeathTimer, this, &ThisClass::BeginDeathFade, Lifetime, false);
}

void UEnemyAttackPresentationComponent::BeginDeathFade()
{
	if (!Death.IsActive() || (Combatant.IsValid() && !Combatant->IsDefeated())) { return; }
	if (Death.BeginFade(Mesh.Get(), PresentationData)) { SetComponentTickEnabled(true); }
	else { FinishDeathPresentation(); }
}

void UEnemyAttackPresentationComponent::UpdateDeathFade(float DeltaTime)
{
	if (Death.AdvanceFade(DeltaTime)) { FinishDeathPresentation(); }
}

void UEnemyAttackPresentationComponent::FinishDeathPresentation()
{
	if (!Death.IsActive() || !GetOwner() || (Combatant.IsValid() && !Combatant->IsDefeated())) { return; }
	Death.Hide(*GetOwner());
	SetComponentTickEnabled(false);
}

void UEnemyAttackPresentationComponent::ResetDeathPresentation()
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(DeathTimer); }
	if (!Death.IsActive()) { return; }
	Death.Restore(GetOwner(), Mesh.Get());
	StopPresentation();
}

void UEnemyAttackPresentationComponent::PlayHitFeedback(const FEnemyAttackHit& Hit,const EBattleDefenseResult Result)
{
	if (Result==EBattleDefenseResult::DodgeSuccess || Result==EBattleDefenseResult::ParrySuccess) { return; }
	const auto* Battle=GetWorld()->GetSubsystem<UBattleCoordinator>();
	const AActor* Target=Battle->GetActivePlayerPawn();
	const FVector Location=Target?Target->GetActorLocation():GetOwner()->GetActorLocation();
	CombatSoundPlayback::Play(GetOwner(),Hit.ImpactSound,Location,Hit.SoundVolume,Hit.SoundDelay,Hit.SoundStartTime);
	if (Hit.ImpactEffect) { UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),Hit.ImpactEffect,Location,FRotator::ZeroRotator,FVector(Hit.EffectScale)); }
}
void UEnemyAttackPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ResetDeathPresentation();
	StopPresentation();
	if (Combatant.IsValid()) { Combatant->OnHealthChanged().Remove(HealthHandle); }
	Super::EndPlay(Reason);
}
