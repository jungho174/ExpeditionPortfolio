#include "UI/PlayerUtilityPresentationComponent.h"
#include "UI/PlayerUtilityPresentationData.h"
#include "UI/MeleeActionData.h"
#include "UI/EnemyAttackPresentationComponent.h"
#include "UI/EnemyPresentationData.h"
#include "Combat/BattleCoordinator.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UI/CombatSoundPlayback.h"

UPlayerUtilityPresentationComponent::UPlayerUtilityPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

float UPlayerUtilityPresentationComponent::GetExpectedDuration(EBattlePlayerCommand Command) const
{
	const bool bPotion = Command == EBattlePlayerCommand::UsePotion;
	if (!PresentationData || !PresentationData->Validate(bPotion)) { return 0; }
	return bPotion ? PresentationData->PotionDuration : PresentationData->ApproachDuration
		+ PresentationData->MeleeMontage->GetPlayLength() + PresentationData->ReturnDuration;
}

bool UPlayerUtilityPresentationComponent::BeginAction(EBattlePlayerCommand Command, int64 ActionId, AActor* Target)
{
 if (Command==EBattlePlayerCommand::MeleeAttack && PresentationData)
 {
  auto* Data=NewObject<UMeleeActionData>(this);
  Data->AttackMontage=PresentationData->MeleeMontage; Data->ApproachMontage=PresentationData->ApproachMontage; Data->ReturnMontage=PresentationData->ReturnMontage;
  Data->HitTime=PresentationData->HitTime; Data->ApproachDuration=PresentationData->ApproachDuration; Data->ReturnDuration=PresentationData->ReturnDuration;
  Data->TargetDistance=PresentationData->TargetDistance; Data->ApproachLeapHeight=PresentationData->ApproachLeapHeight; Data->ReturnLeapHeight=PresentationData->ReturnLeapHeight;
  Data->ApproachSound=PresentationData->ApproachSound; Data->ReturnSound=PresentationData->ReturnSound;
  Data->HitSound.Sound=PresentationData->MeleeHitSound; Data->HitSound.Volume=PresentationData->MeleeHitSoundVolume;
  Data->HitSound.Delay=PresentationData->MeleeHitSoundDelay; Data->HitSound.StartTime=PresentationData->MeleeHitSoundStartTime;
  return BeginMelee(Data,ActionId,Target,false);
 }
 StopPresentation();
 ACharacter* Character = Cast<ACharacter>(GetOwner());
	Mesh = Character ? Character->GetMesh() : nullptr;
	const bool bPotion = Command == EBattlePlayerCommand::UsePotion;
	if (!GetExpectedDuration(Command) || !Mesh.IsValid() || !Mesh->GetAnimInstance() || !IsValid(Target)) { return false; }
	ActiveId = ActionId;
	bPotionAction = bPotion;
	ActionTarget = Target;
	if (bPotion)
	{
		USkeletalMeshComponent* RecipientMesh=Target->FindComponentByClass<USkeletalMeshComponent>();
		if (PresentationData->PotionEffect)
		{
			PotionFX = UNiagaraFunctionLibrary::SpawnSystemAttached(PresentationData->PotionEffect, RecipientMesh?RecipientMesh:Mesh.Get(), NAME_None,
				FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
		}
		GetWorld()->GetTimerManager().SetTimer(EffectTimer, FTimerDelegate::CreateUObject(this, &ThisClass::ApplyEffect, ActiveId), PresentationData->PotionApplyTime, false);
		GetWorld()->GetTimerManager().SetTimer(FinishTimer, FTimerDelegate::CreateUObject(this, &ThisClass::Finish, false), PresentationData->PotionDuration, false);
		return true;
	}
 return false;
}

bool UPlayerUtilityPresentationComponent::SupportsCharacterAction_Implementation(UCharacterActionData* Action) const
{ return Cast<UMeleeActionData>(Action)!=nullptr; }
float UPlayerUtilityPresentationComponent::GetCharacterActionDuration_Implementation(UCharacterActionData* Action, bool bCounter) const
{
 const auto* Data=Cast<UMeleeActionData>(Action);
 if (!Data || !Data->IsValidPresentation() || (bCounter && Data->bEnableTimingBonus)) { return 0; }
 return Data->ApproachDuration+Data->AttackMontage->GetPlayLength()+Data->ReturnDuration
  + (!bCounter && Data->bEnableTimingBonus?Data->GetPreparationDuration():0);
}
bool UPlayerUtilityPresentationComponent::StartCharacterAction_Implementation(UCharacterActionData* Action, int64 ActionId, AActor* Target, bool bCounter)
{ return BeginMelee(Cast<UMeleeActionData>(Action),ActionId,Target,bCounter); }
bool UPlayerUtilityPresentationComponent::BeginMelee(UMeleeActionData* Data, int64 ActionId, AActor* Target, bool bCounter)
{
 StopPresentation();
 const auto* Character=Cast<ACharacter>(GetOwner());
 Mesh=Character?Character->GetMesh():nullptr;
 if (GetCharacterActionDuration_Implementation(Data,bCounter)<=0 || !Mesh.IsValid() || !Mesh->GetAnimInstance() || !IsValid(Target)) { return false; }
 ActiveMeleeData=Data; ActiveId=ActionId; ActionTarget=Target; bCounterAction=bCounter; bPotionAction=false;
 // During a counter the enemy mesh can still be at its attack position, away from its collision actor.
 const auto* TargetMesh=Target->FindComponentByClass<USkeletalMeshComponent>();
 const FVector TargetLocation=TargetMesh?TargetMesh->GetComponentLocation():Target->GetActorLocation();
 FVector Direction=TargetLocation-GetOwner()->GetActorLocation(); Direction.Z=0;
 if (!Direction.IsNearlyZero()) { GetOwner()->SetActorRotation(Direction.Rotation()); }
 OriginalTransform=Mesh->GetRelativeTransform(); bMeshSaved=true;
 float TargetDistance=Data->TargetDistance;
 const auto* Enemy=Target->FindComponentByClass<UEnemyAttackPresentationComponent>();
 if (Enemy && Enemy->PresentationData && FMath::IsFinite(Enemy->PresentationData->PlayerMeleeDistance))
 { TargetDistance=FMath::Max(TargetDistance,Enemy->PresentationData->PlayerMeleeDistance); }
 const FVector Travel=Direction.GetSafeNormal()*FMath::Max(0.f,Direction.Size()-TargetDistance);
 TravelHome=GetOwner()->GetActorLocation(); TravelMeshHome=Mesh->GetComponentLocation();
 FCollisionQueryParams Query(SCENE_QUERY_STAT(PlayerMeleeGroundTravel),true,GetOwner());
 Query.AddIgnoredActor(Target); Query.AddIgnoredActor(GetOwner()->GetAttachParentActor());
 if (const auto* Battle=GetWorld()->GetSubsystem<UBattleCoordinator>())
 {
  Query.AddIgnoredActors(Battle->GetBattleAllies()); Query.AddIgnoredActors(Battle->GetBattleEnemies());
  Query.AddIgnoredActor(Battle->GetActiveEncounterSource());
 }
 FString Failure;
 if (!UBattleGroundingLibrary::BuildPath(GetWorld(),TravelHome,TravelHome+Travel,UBattleGroundingLibrary::GetCapsuleExtent(GetOwner()),Data->Grounding,Query,GroundTravelPoints,Failure))
 {
  UE_LOG(LogTemp,Warning,TEXT("Melee approach cancelled: Actor=%s Reason=%s"),*GetNameSafe(GetOwner()),*Failure);
  StopPresentation(); return false;
 }
 MoveDuration=Data->ApproachDuration; ReturnTime=Data->ReturnDuration; StrikeTime=Data->HitTime;
 ApproachHeight=Data->ApproachLeapHeight; ReturnHeight=Data->ReturnLeapHeight;
 ActiveMontage=Data->AttackMontage; TravelMontage=Data->ApproachMontage; ReturnMontage=Data->ReturnMontage;
 if (!bCounter && Data->bEnableTimingBonus)
 {
  if (Data->PreparationMontage) { Mesh->GetAnimInstance()->Montage_Play(Data->PreparationMontage); }
  GetWorld()->GetSubsystem<UBattleCoordinator>()->BeginPlayerPreparation(ActiveId,Data->GetQTEDuration(),Data->TimingSuccessDuration,Data->TimingDamageMultiplier,true,Data->QTEPattern);
  GetWorld()->GetTimerManager().SetTimer(PreparationTimer,FTimerDelegate::CreateUObject(this,&ThisClass::FinishPreparation,ActiveId),Data->GetPreparationDuration(),false);
 }
 else { StartApproach(); }
 return ActiveId!=0;
}
void UPlayerUtilityPresentationComponent::FinishPreparation(int64 ExpectedId)
{
 if (!ActiveId || ActiveId!=ExpectedId) { return; }
 GetWorld()->GetSubsystem<UBattleCoordinator>()->NotifyPlayerPreparationFinished(ActiveId);
 StartApproach();
}
void UPlayerUtilityPresentationComponent::StartApproach()
{
 if (!PlayTravelMontage()) { Finish(true); return; }
 Elapsed=0; bReturning=false; SetComponentTickEnabled(true);
 const auto& Sound=ActiveMeleeData->ApproachSound;
 CombatSoundPlayback::Play(GetOwner(),Sound.Sound,Mesh->GetComponentLocation(),Sound.Volume,Sound.Delay,Sound.StartTime);
}

void UPlayerUtilityPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (!ActiveId || !Mesh.IsValid()) { Finish(true); return; }
	Elapsed += DeltaTime;
	const float Alpha = FMath::Clamp(Elapsed / MoveDuration, 0.0f, 1.0f);
	const float Smooth = Alpha * Alpha * (3 - 2 * Alpha);
	const FVector Leap = FVector::UpVector * FMath::Sin(PI * Alpha) * (bReturning ? ReturnHeight : ApproachHeight);
	const FVector Root = UBattleGroundingLibrary::EvaluatePath(GroundTravelPoints, bReturning ? 1 - Smooth : Smooth);
	Mesh->SetWorldLocation(TravelMeshHome + (Root - TravelHome) + Leap);
	if (Alpha >= 1)
	{
		SetComponentTickEnabled(false);
		if (bReturning) { Finish(false); }
		else { StartMelee(); }
	}
}

void UPlayerUtilityPresentationComponent::StartMelee()
{
	UnbindTravelMontage();
	UAnimInstance* Anim = Mesh.IsValid() ? Mesh->GetAnimInstance() : nullptr;
	if (!Anim || Anim->Montage_Play(ActiveMontage) <= 0) { Finish(true); return; }
	FOnMontageEnded Delegate;
	Delegate.BindUObject(this, &ThisClass::OnMontageEnded, ActiveId);
	Anim->Montage_SetEndDelegate(Delegate, ActiveMontage);
	FOnMontageBlendingOutStarted BlendingOut;
	BlendingOut.BindUObject(this, &ThisClass::OnMontageBlendingOut, ActiveId);
	Anim->Montage_SetBlendingOutDelegate(BlendingOut, ActiveMontage);
	GetWorld()->GetTimerManager().SetTimer(EffectTimer, FTimerDelegate::CreateUObject(this, &ThisClass::ApplyEffect, ActiveId), StrikeTime, false);
}

void UPlayerUtilityPresentationComponent::ApplyEffect(int64 ExpectedId)
{
	if (ExpectedId && ExpectedId == ActiveId)
	{
		auto* Battle=GetWorld()->GetSubsystem<UBattleCoordinator>();
		const bool bApplied=bCounterAction?Battle->NotifyParryCounterHit(ExpectedId):Battle->NotifyPlayerActionHit(ExpectedId);
		if (bApplied)
		{
			if (bPotionAction)
			{
				CombatSoundPlayback::Play(GetOwner(), PresentationData->PotionSound, ActionTarget.IsValid()?ActionTarget->GetActorLocation():GetOwner()->GetActorLocation(),
					PresentationData->PotionSoundVolume, PresentationData->PotionSoundDelay, PresentationData->PotionSoundStartTime);
			}
			else
			{
    const auto& Sound=ActiveMeleeData->HitSound;
    CombatSoundPlayback::Play(GetOwner(),Sound.Sound,ActionTarget.IsValid()?ActionTarget->GetActorLocation():GetOwner()->GetActorLocation(),Sound.Volume,Sound.Delay,Sound.StartTime);
			}
		}
	}
}

void UPlayerUtilityPresentationComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, int64 ExpectedId)
{
	if (!ActiveId || ExpectedId != ActiveId || Montage != ActiveMontage || bReturning) { return; }
	if (bInterrupted) { Finish(true); return; }
	GetWorld()->GetTimerManager().ClearTimer(EffectTimer);
	ApplyEffect(ExpectedId); // Same-ID duplicate protection also covers an unusually delayed hit timer.
	const auto* Battle=GetWorld()->GetSubsystem<UBattleCoordinator>();
	if (Battle->GetCurrentState()==EBattleState::Victory || Battle->GetCurrentState()==EBattleState::Defeat) { return; }
	bReturning = true;
	TravelMontage = ReturnMontage;
	Elapsed = 0;
	MoveDuration = ReturnTime;
	if (!PlayTravelMontage()) { Finish(true); return; }
	SetComponentTickEnabled(true);
	const auto& Sound=ActiveMeleeData->ReturnSound;
	CombatSoundPlayback::Play(GetOwner(),Sound.Sound,Mesh->GetComponentLocation(),Sound.Volume,Sound.Delay,Sound.StartTime);
}

bool UPlayerUtilityPresentationComponent::PlayTravelMontage()
{
	if (!TravelMontage) { return true; }
	UAnimInstance* Anim = Mesh.IsValid() ? Mesh->GetAnimInstance() : nullptr;
	if (!Anim || MoveDuration <= 0 || TravelMontage->GetPlayLength() <= 0
		|| Anim->Montage_Play(TravelMontage, TravelMontage->GetPlayLength() / MoveDuration) <= 0) { return false; }
	FOnMontageBlendingOutStarted Delegate;
	Delegate.BindUObject(this, &ThisClass::OnTravelBlendingOut, ActiveId);
	Anim->Montage_SetBlendingOutDelegate(Delegate, TravelMontage);
	return true;
}

void UPlayerUtilityPresentationComponent::UnbindTravelMontage()
{
	if (TravelMontage && Mesh.IsValid() && Mesh->GetAnimInstance())
	{
		FOnMontageBlendingOutStarted Empty;
		Mesh->GetAnimInstance()->Montage_SetBlendingOutDelegate(Empty, TravelMontage);
	}
}

void UPlayerUtilityPresentationComponent::OnTravelBlendingOut(UAnimMontage* Montage, bool bInterrupted, int64 ExpectedId)
{
	if (bInterrupted && ActiveId && ExpectedId == ActiveId && Montage == TravelMontage) { Finish(true); }
}

void UPlayerUtilityPresentationComponent::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, int64 ExpectedId)
{
	// Cancel at interruption start: a long blend-out must not leave the hit timer armed.
	if (bInterrupted && ActiveId && ExpectedId == ActiveId && Montage == ActiveMontage) { Finish(true); }
}

void UPlayerUtilityPresentationComponent::Finish(bool bInterrupted)
{
	const int64 FinishedId = ActiveId;
 const bool bWasCounter=bCounterAction;
 StopPresentation();
 if (FinishedId && GetWorld())
 {
  auto* Battle=GetWorld()->GetSubsystem<UBattleCoordinator>();
  if (bWasCounter) { Battle->NotifyParryCounterFinished(FinishedId,bInterrupted); }
  else { Battle->NotifyPlayerActionFinished(FinishedId,bInterrupted); }
 }
}

void UPlayerUtilityPresentationComponent::RestoreMesh()
{
	if (bMeshSaved && Mesh.IsValid()) { Mesh->SetRelativeTransform(OriginalTransform); }
	bMeshSaved = false;
}

void UPlayerUtilityPresentationComponent::StopPresentation()
{
	ActiveId = 0;
	UnbindTravelMontage();
	SetComponentTickEnabled(false);
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PreparationTimer);
		GetWorld()->GetTimerManager().ClearTimer(EffectTimer);
		GetWorld()->GetTimerManager().ClearTimer(FinishTimer);
	}
	if (Mesh.IsValid() && Mesh->GetAnimInstance() && ActiveMontage)
	{
		if (ActiveMeleeData && ActiveMeleeData->PreparationMontage)
		{
			Mesh->GetAnimInstance()->Montage_Stop(0.1f,ActiveMeleeData->PreparationMontage);
		}
		FOnMontageEnded Empty;
		Mesh->GetAnimInstance()->Montage_SetEndDelegate(Empty, ActiveMontage);
		FOnMontageBlendingOutStarted EmptyBlendingOut;
		Mesh->GetAnimInstance()->Montage_SetBlendingOutDelegate(EmptyBlendingOut, ActiveMontage);
		Mesh->GetAnimInstance()->Montage_Stop(0.1f, ActiveMontage);
	}
	RestoreMesh();
	GroundTravelPoints.Reset();
	if (Mesh.IsValid() && Mesh->GetAnimInstance() && TravelMontage) { Mesh->GetAnimInstance()->Montage_Stop(0.1f, TravelMontage); }
	if (PotionFX) { PotionFX->DestroyComponent(); PotionFX = nullptr; }
	ActiveMontage = nullptr;
	TravelMontage = nullptr;
	ReturnMontage = nullptr;
	ActiveMeleeData=nullptr; bCounterAction=false; bPotionAction=false;
	Mesh.Reset();
}

void UPlayerUtilityPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StopPresentation();
	Super::EndPlay(Reason);
}
