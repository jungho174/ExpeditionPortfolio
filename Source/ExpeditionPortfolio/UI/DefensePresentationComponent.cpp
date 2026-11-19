#include "UI/DefensePresentationComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "HAL/PlatformTime.h"
#include "UI/DefensePresentationData.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/BattleCoordinator.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "UI/CombatSoundPlayback.h"

UDefensePresentationComponent::UDefensePresentationComponent()
{
	// Only the continuous retreat/return visual needs per-frame interpolation.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UDefensePresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner())) { Mesh = Character->GetMesh(); }
	Battle = GetWorld()->GetSubsystem<UBattleCoordinator>();
	if (Battle.IsValid())
	{
		AcceptedHandle = Battle->OnDefenseCommandAccepted().AddUObject(this, &ThisClass::HandleDefenseAccepted);
		ResultHandle = Battle->OnDefenseResultChanged().AddUObject(this, &ThisClass::HandleDefenseResult);
		StateHandle = Battle->OnBattleStateChanged().AddUObject(this, &ThisClass::HandleBattleState);
	}
}

bool UDefensePresentationComponent::PlayMontage(UAnimMontage* Montage, const float Rate, const float StartTime)
{
	if (!Mesh.IsValid() || !Mesh->GetAnimInstance() || !Montage) { return false; }
	UAnimInstance* Anim = Mesh->GetAnimInstance();
	if (Anim->Montage_Play(Montage, Rate, EMontagePlayReturnType::MontageLength, StartTime) <= 0.0f) { return false; }
	ActiveMontage = Montage;
	FOnMontageEnded Ended;
	Ended.BindUObject(this, &ThisClass::OnMontageEnded);
	Anim->Montage_SetEndDelegate(Ended, Montage);
	return true;
}

void UDefensePresentationComponent::HandleDefenseAccepted(const EBattleDefenseCommand Command, const float TimeUntilHit)
{
	if (!Battle.IsValid() || Battle->GetActivePlayerPawn() != GetOwner()) { return; }
	StopPresentation();
	FString Reason;
	if (!PresentationData || !PresentationData->Validate(Reason) || !Mesh.IsValid())
	{
		UE_LOG(LogBattleCoordinator, Warning, TEXT("Defense presentation unavailable: %s"), *Reason);
		return;
	}
	if (Command == EBattleDefenseCommand::Parry)
	{
		if (PlayMontage(PresentationData->ParryMontage, PresentationData->ParryPlayRate, PresentationData->ParryStartTime))
		{
			GetWorld()->GetTimerManager().SetTimer(GuardTimer, this, &ThisClass::StopGuard, PresentationData->ParryDuration, false);
		}
		return;
	}
	RetreatDuration = FMath::Min(PresentationData->DodgeRetreatDuration, FMath::Max(0.01f, TimeUntilHit * 0.8f));
	PoseTime = PresentationData->DodgePoseTime;
	if (!PlayMontage(PresentationData->DodgeMontage, PoseTime / RetreatDuration)) { return; }
	OriginalMeshTransform = Mesh->GetRelativeTransform();
	const float TravelDistance = PrepareDodgePath();
	ReturnStartTime = FMath::Max(RetreatDuration, TimeUntilHit + PresentationData->DodgeHoldAfterHit);
	ReturnDuration = PresentationData->DodgeReturnDuration;
	DodgeElapsed = 0.0f;
	bDodging = true;
	SetComponentTickEnabled(true);
	const auto& Sound=PresentationData->DodgeSound;
	CombatSoundPlayback::Play(GetOwner(),Sound.Sound,Mesh->GetComponentLocation(),Sound.Volume,Sound.Delay,Sound.StartTime);
	UE_LOG(LogBattleCoordinator, Log, TEXT("Dodge presentation started: Distance=%.1f ActualDistance=%.1f ReturnAt=%.3f"), PresentationData->DodgeDistance, TravelDistance, ReturnStartTime);
}

float UDefensePresentationComponent::PrepareDodgePath()
{
	DodgeHome=GetOwner()->GetActorLocation();DodgeMeshHome=Mesh->GetComponentLocation();
	DodgeGroundPoints={DodgeHome};
	if (PresentationData->DodgeDistance<=KINDA_SMALL_NUMBER) { return 0; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DodgeGroundTravel),true,GetOwner());
	Query.AddIgnoredActor(GetOwner()->GetAttachParentActor());
	if (Battle.IsValid())
	{
		Query.AddIgnoredActors(Battle->GetBattleAllies());Query.AddIgnoredActors(Battle->GetBattleEnemies());
		Query.AddIgnoredActor(Battle->GetActiveEncounterSource());
	}
	FString Failure;
	const FVector Goal=DodgeHome-GetOwner()->GetActorForwardVector()*PresentationData->DodgeDistance;
	if (!UBattleGroundingLibrary::BuildPath(GetWorld(),DodgeHome,Goal,UBattleGroundingLibrary::GetCapsuleExtent(GetOwner()),
		PresentationData->DodgeGrounding,Query,DodgeGroundPoints,Failure,true))
	{
		// Keep the accepted defense and montage even if no safe displacement is available.
		DodgeGroundPoints={DodgeHome};
	}
	return FVector::Dist2D(DodgeHome,DodgeGroundPoints.Last());
}

void UDefensePresentationComponent::HandleDefenseResult(const EBattleDefenseResult Result)
{
	if (!Battle.IsValid() || Battle->GetActivePlayerPawn() != GetOwner()) { return; }
	if (Result == EBattleDefenseResult::Failed || Result == EBattleDefenseResult::NoInput)
	{
		if (PresentationData)
		{
			const auto& Sound=PresentationData->HitSound;
			CombatSoundPlayback::Play(GetOwner(),Sound.Sound,GetOwner()->GetActorLocation(),Sound.Volume,Sound.Delay,Sound.StartTime);
		}
		StopPresentation();
		FString Reason;
		if (PresentationData && PresentationData->Validate(Reason))
		{
			PlayMontage(PresentationData->HitReactionMontage, PresentationData->HitReactionPlayRate);
		}
	}
}

void UDefensePresentationComponent::HandleBattleState(EBattleState Previous, const EBattleState Current)
{
	if (Current == EBattleState::PlayerAction || Current == EBattleState::PlayerCommand
		|| Current == EBattleState::ReturnToField || Current == EBattleState::Exploration || Current == EBattleState::EnemyTelegraph)
	{
		StopPresentation();
	}
}

void UDefensePresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateDodge(DeltaTime);
}

void UDefensePresentationComponent::UpdateDodge(const float DeltaTime)
{
	if (!bDodging || !Mesh.IsValid()) { StopPresentation(); return; }
	DodgeElapsed += DeltaTime;
	float Alpha = 1.0f;
	UAnimInstance* Anim = Mesh->GetAnimInstance();
	if (DodgeElapsed < RetreatDuration) { Alpha = FMath::SmoothStep(0.0f, RetreatDuration, DodgeElapsed); }
	else if (DodgeElapsed < ReturnStartTime)
	{
		if (!bHoldingPose)
		{
			bHoldingPose = true;
			if (Anim && ActiveMontage)
			{
				Anim->Montage_SetPosition(ActiveMontage, PoseTime);
				Anim->Montage_Pause(ActiveMontage);
			}
		}
	}
	else
	{
		if (!bReturning)
		{
			bReturning = true;
			if (Anim && ActiveMontage)
			{
				FOnMontageEnded Empty;
				Anim->Montage_SetEndDelegate(Empty, ActiveMontage);
				Anim->Montage_Resume(ActiveMontage);
				Anim->Montage_Stop(ReturnDuration, ActiveMontage);
				ActiveMontage = nullptr;
			}
		}
		Alpha = 1.0f - FMath::SmoothStep(ReturnStartTime, ReturnStartTime + ReturnDuration, DodgeElapsed);
	}
	const FVector Root=UBattleGroundingLibrary::EvaluatePath(DodgeGroundPoints,Alpha);
	Mesh->SetWorldLocation(DodgeMeshHome+(Root-DodgeHome));
	if (DodgeElapsed >= ReturnStartTime + ReturnDuration) { StopPresentation(); }
}

void UDefensePresentationComponent::StopGuard()
{
	StopPresentation();
}

void UDefensePresentationComponent::OnMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (Montage != ActiveMontage) { return; }
	StopPresentation();
}

void UDefensePresentationComponent::StopPresentation()
{
	SetComponentTickEnabled(false);
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(GuardTimer); }
	if (bDodging && Mesh.IsValid()) { Mesh->SetRelativeTransform(OriginalMeshTransform); }
	DodgeGroundPoints.Reset();
	bDodging = false;
	bHoldingPose = false;
	bReturning = false;
	if (Mesh.IsValid() && Mesh->GetAnimInstance() && ActiveMontage)
	{
		FOnMontageEnded Empty;
		Mesh->GetAnimInstance()->Montage_SetEndDelegate(Empty, ActiveMontage);
		Mesh->GetAnimInstance()->Montage_Stop(0.12f, ActiveMontage);
	}
	ActiveMontage = nullptr;
}

void UDefensePresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Battle.IsValid())
	{
		Battle->OnDefenseCommandAccepted().Remove(AcceptedHandle);
		Battle->OnDefenseResultChanged().Remove(ResultHandle);
		Battle->OnBattleStateChanged().Remove(StateHandle);
	}
	StopPresentation();
	Battle.Reset();
	Super::EndPlay(Reason);
}
