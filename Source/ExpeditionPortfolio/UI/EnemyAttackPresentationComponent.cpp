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
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

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
	if (bDeathActive || (Combatant.IsValid() && Combatant->IsDefeated())) { return false; }
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
	if (bDeathFading) { UpdateDeathFade(DeltaTime); return; }
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
	ActiveId=0; bTravel=false; bHeldForCounter=false; SetComponentTickEnabled(bDeathFading);
	if (Mesh.IsValid() && Mesh->GetAnimInstance() && ActiveMontage)
	{
		FOnMontageEnded Empty; FOnMontageBlendingOutStarted EmptyBlend;
		Mesh->GetAnimInstance()->Montage_SetEndDelegate(Empty,ActiveMontage);
		Mesh->GetAnimInstance()->Montage_SetBlendingOutDelegate(EmptyBlend,ActiveMontage);
		Mesh->GetAnimInstance()->Montage_Stop(.1f,ActiveMontage);
	}
	if (!bDeathActive)
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
	if (After>0 && bDeathActive) { ResetDeathPresentation(); }
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
	if (bDeathActive || !GetOwner() || !GetWorld()) { return; }
	bDeathActive=true;
	bWasHiddenBeforeDeath=GetOwner()->IsHidden();
	bCollisionBeforeDeath=GetOwner()->GetActorEnableCollision();
	GetOwner()->SetActorEnableCollision(false);
	if (Mesh.IsValid() && !bSavedTransform) { OriginalTransform=Mesh->GetRelativeTransform(); bSavedTransform=true; }
	// Cancel attack callbacks while keeping the mesh at the killing-blow location.
	StopPresentation();
	const auto Duration=[](float Value,float Fallback,float Min,float Max)
	{ return FMath::IsFinite(Value)?FMath::Clamp(Value,Min,Max):Fallback; };
	float Lifetime=Duration(PresentationData?PresentationData->DeathFallbackDuration:.75f,.75f,.05f,30.f);
	DeathMontage=PresentationData?PresentationData->Death.Get():nullptr;
	if (Mesh.IsValid() && Mesh->GetAnimInstance())
	{
		auto* Anim=Mesh->GetAnimInstance();
		Anim->StopAllMontages(.05f);
		if (DeathMontage)
		{
			const float Rate=Duration(PresentationData->DeathPlayRate,1.f,.05f,10.f);
			const float Played=Anim->Montage_Play(DeathMontage,Rate,EMontagePlayReturnType::Duration);
			if (Played>0 && FMath::IsFinite(Played))
			{
				Lifetime=Played;
				// Set only this instance: never change the shared montage asset at runtime.
				if (auto* Instance=Anim->GetActiveInstanceForMontage(DeathMontage)) { Instance->bEnableAutoBlendOut=false; }
			}
		}
	}
	Lifetime+=Duration(PresentationData?PresentationData->DeathDisappearDelay:.2f,.2f,0.f,30.f);
	if (PresentationData)
	{
		const auto& Sound=PresentationData->DeathSound;
		CombatSoundPlayback::Play(GetOwner(),Sound.Sound,Mesh.IsValid()?Mesh->GetComponentLocation():GetOwner()->GetActorLocation(),Sound.Volume,Sound.Delay,Sound.StartTime);
	}
	GetWorld()->GetTimerManager().SetTimer(DeathTimer,this,&ThisClass::BeginDeathFade,Lifetime,false);
	UE_LOG(LogTemp,Log,TEXT("Enemy death started: Actor=%s Montage=%s FadeStartsAfter=%.2f"),*GetNameSafe(GetOwner()),*GetNameSafe(DeathMontage),Lifetime);
}

void UEnemyAttackPresentationComponent::BeginDeathFade()
{
	if (!bDeathActive || (Combatant.IsValid() && !Combatant->IsDefeated())) { return; }
	const float Config=PresentationData?PresentationData->DeathFadeDuration:0;
	DeathFadeDuration=FMath::IsFinite(Config)?FMath::Clamp(Config,0.f,30.f):.8f;
	DeathFadeParameter=PresentationData?PresentationData->DeathFadeParameter:NAME_None;
	if (DeathFadeDuration<=0 || !Mesh.IsValid() || Mesh->GetNumMaterials()==0 || DeathFadeParameter.IsNone()) { FinishDeathPresentation(); return; }
	TArray<UMaterialInterface*> Sources;
	for (int32 Index=0;Index<Mesh->GetNumMaterials();++Index)
	{
		UMaterialInterface* Material=PresentationData->DeathFadeMaterials.IsValidIndex(Index) && PresentationData->DeathFadeMaterials[Index]
			? PresentationData->DeathFadeMaterials[Index].Get() : Mesh->GetMaterial(Index);
		float Value=1;
		if (!Material || !Material->GetScalarParameterValue(FMaterialParameterInfo(DeathFadeParameter),Value))
		{
			UE_LOG(LogTemp,Warning,TEXT("Enemy death fade unavailable: Actor=%s Slot=%d Parameter=%s. Configure a fade material in the enemy presentation DA."),*GetNameSafe(GetOwner()),Index,*DeathFadeParameter.ToString());
			FinishDeathPresentation(); return;
		}
		Sources.Add(Material);
	}
	SavedDeathMaterials.Reset();DeathFadeInstances.Reset();
	for (int32 Index=0;Index<Sources.Num();++Index)
	{
		SavedDeathMaterials.Add(Mesh->GetMaterial(Index));
		auto* Instance=UMaterialInstanceDynamic::Create(Sources[Index],this);
		if (!Instance) { RestoreDeathMaterials();FinishDeathPresentation();return; }
		Instance->SetScalarParameterValue(DeathFadeParameter,1.f);
		DeathFadeInstances.Add(Instance);Mesh->SetMaterial(Index,Instance);
	}
	DeathFadeElapsed=0;bDeathFading=true;SetComponentTickEnabled(true);
	UE_LOG(LogTemp,Log,TEXT("Enemy death fade started: Actor=%s Duration=%.2f"),*GetNameSafe(GetOwner()),DeathFadeDuration);
}

void UEnemyAttackPresentationComponent::UpdateDeathFade(float DeltaTime)
{
	if (!bDeathFading) { return; }
	DeathFadeElapsed+=FMath::Max(0.f,DeltaTime);
	const float Alpha=FMath::Clamp(DeathFadeElapsed/DeathFadeDuration,0.f,1.f);
	const float Opacity=1-Alpha*Alpha*(3-2*Alpha);
	for (UMaterialInstanceDynamic* Instance:DeathFadeInstances) { if (Instance) { Instance->SetScalarParameterValue(DeathFadeParameter,Opacity); } }
	if (Alpha>=1) { FinishDeathPresentation(); }
}

void UEnemyAttackPresentationComponent::RestoreDeathMaterials()
{
	if (Mesh.IsValid())
	{
		for (int32 Index=0;Index<SavedDeathMaterials.Num();++Index) { Mesh->SetMaterial(Index,SavedDeathMaterials[Index]); }
	}
	SavedDeathMaterials.Reset();DeathFadeInstances.Reset();
}

void UEnemyAttackPresentationComponent::FinishDeathPresentation()
{
	if (!bDeathActive || !GetOwner() || (Combatant.IsValid() && !Combatant->IsDefeated())) { return; }
	bDeathFading=false;SetComponentTickEnabled(false);
	// Keep the instance for entry-HP restoration on retry/retreat; victory Continue destroys it.
	GetOwner()->SetActorHiddenInGame(true);
	UE_LOG(LogTemp,Log,TEXT("Enemy disappeared: Actor=%s"),*GetNameSafe(GetOwner()));
}

void UEnemyAttackPresentationComponent::ResetDeathPresentation()
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(DeathTimer); }
	if (!bDeathActive) { return; }
	bDeathFading=false;RestoreDeathMaterials();
	if (Mesh.IsValid() && Mesh->GetAnimInstance() && DeathMontage) { Mesh->GetAnimInstance()->Montage_Stop(0.f,DeathMontage); }
	DeathMontage=nullptr;
	bDeathActive=false;
	StopPresentation();
	if (GetOwner())
	{
		GetOwner()->SetActorHiddenInGame(bWasHiddenBeforeDeath);
		GetOwner()->SetActorEnableCollision(bCollisionBeforeDeath);
	}
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
