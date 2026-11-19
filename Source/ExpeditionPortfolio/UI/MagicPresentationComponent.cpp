#include "UI/MagicPresentationComponent.h"
#include "Combat/CombatantVisualData.h"
#include "UI/MagicActionData.h"
#include "UI/DefensePresentationComponent.h"
#include "UI/EnemyAttackPresentationComponent.h"
#include "Combat/BattleCoordinator.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "CoreGlobals.h"
#include "Containers/Ticker.h"
#include "UI/BattleSlowMotionSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"
#include "Sound/SoundBase.h"
#include "UI/CombatSoundPlayback.h"

UMagicPresentationComponent::UMagicPresentationComponent()
{
	// Only moving charge anchors or an active projectile need per-frame updates.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

bool UMagicPresentationComponent::SupportsCharacterAction_Implementation(UCharacterActionData* Action) const
{
 return Cast<UMagicActionData>(Action)!=nullptr;
}
float UMagicPresentationComponent::GetCharacterActionDuration_Implementation(UCharacterActionData* Action, bool bCounter) const
{
 const auto* Data=Cast<UMagicActionData>(Action);
 if (!Data || !Data->AttackMontage || !Data->ValidateProjectile()
  || !FMath::IsFinite(Data->AttackPlayRate) || Data->AttackPlayRate<=0
  || !FMath::IsFinite(Data->AttackStartTime) || Data->AttackStartTime<0
  || Data->AttackStartTime>=Data->AttackMontage->GetPlayLength()
  || (bCounter && (Data->ChargeMontage || Data->bEnableTimingBonus))) { return 0; }
 return (Data->AttackMontage->GetPlayLength()-Data->AttackStartTime)/Data->AttackPlayRate
  + (!bCounter && (Data->ChargeMontage || Data->bEnableTimingBonus)?Data->GetPreparationDuration():0)
  + (Data->bUseProjectile?Data->MaxFlightDuration:0);
}
UCharacterActionData* UMagicPresentationComponent::GetLegacyAction_Implementation(EBattlePlayerCommand Command) const
{
 return Command==EBattlePlayerCommand::PowerStrike?StrongMagic.Get():Command==EBattlePlayerCommand::BasicAttack?BasicMagic.Get():nullptr;
}
bool UMagicPresentationComponent::StartCharacterAction_Implementation(UCharacterActionData* Action, int64 ActionId, AActor* Target, bool bCounter)
{
 return GetCharacterActionDuration_Implementation(Action,bCounter)>0 && BeginAction(Cast<UMagicActionData>(Action),ActionId,Target,bCounter);
}
float UMagicPresentationComponent::GetExpectedDuration(EBattlePlayerCommand Command) const
{
 return GetCharacterActionDuration_Implementation(GetLegacyAction_Implementation(Command),false);
}

float UMagicPresentationComponent::GetCounterDuration() const
{
	const UMagicActionData* Data = CounterMagic;
	if (!Data || !Data->AttackMontage || Data->ChargeMontage || !Data->bUseProjectile || !Data->ValidateProjectile()
		|| !FMath::IsFinite(Data->AttackPlayRate) || Data->AttackPlayRate <= 0
		|| !FMath::IsFinite(Data->AttackStartTime) || Data->AttackStartTime < 0
		|| Data->AttackStartTime >= Data->AttackMontage->GetPlayLength()) { return 0; }
	if (!FMath::IsFinite(Data->ParrySlowDuration) || Data->ParrySlowDuration < 0 || Data->ParrySlowDuration > 1
		|| !FMath::IsFinite(Data->ParrySlowFactor) || Data->ParrySlowFactor < 0.01f || Data->ParrySlowFactor > 1) { return 0; }
	return Data->ParrySlowDuration + (Data->AttackMontage->GetPlayLength() - Data->AttackStartTime) / Data->AttackPlayRate
		+ (Data->bUseProjectile ? Data->MaxFlightDuration : 0.0f);
}

float UMagicPresentationComponent::GetCounterActionDamage_Implementation() const
{
	return CounterMagic ? CounterMagic->BaseDamage : 0.0f;
}

bool UMagicPresentationComponent::BeginCounterAction(const int64 ActionId, AActor* Target)
{
	StopPresentation();
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	Mesh = Character ? Character->GetMesh() : nullptr;
	if (GetCounterDuration() <= 0 || !Mesh.IsValid() || !Mesh->GetAnimInstance() || !IsValid(Target)) { return false; }
	if (CounterMagic->ParrySlowDuration <= 0)
	{
		if (auto* Defense = GetOwner()->FindComponentByClass<UDefensePresentationComponent>()) { Defense->StopPresentation(); }
		return BeginAction(CounterMagic, ActionId, Target, true);
	}
	ActiveData = CounterMagic;
	ActiveId = ActionId;
	TargetActor = Target;
	bCounterAction = true;
	bImpactHolding = true;
	ImpactSlowId = GetWorld()->GetSubsystem<UBattleSlowMotionSubsystem>()->Begin(EBattleSlowMotion::Parry, CounterMagic->ParrySlowFactor);
	ImpactEndsAt = FPlatformTime::Seconds() + CounterMagic->ParrySlowDuration;
	const FVector Location = Mesh->GetSocketLocation(CounterMagic->CastSocket);
	if (CounterMagic->ParryImpactEffect)
	{
		ParryImpactFX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), CounterMagic->ParryImpactEffect,
			Location, GetOwner()->GetActorRotation(), CounterMagic->ParryImpactScale);
		if (IsValid(ParryImpactFX))
		{
			ParryImpactFX->SetForceSolo(true);
			ParryImpactFX->SetCustomTimeDilation(1.0f / CounterMagic->ParrySlowFactor);
		}
	}
	CombatSoundPlayback::Play(GetOwner(), CounterMagic->ParryImpactSound, Location,
		CounterMagic->ParrySoundVolume, CounterMagic->ParrySoundDelay, CounterMagic->ParrySoundStartTime);
	const APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (PC && PC->PlayerCameraManager && CounterMagic->ParryCameraShake)
	{
		ImpactCamera = PC->PlayerCameraManager;
		ImpactShake = ImpactCamera->StartCameraShake(CounterMagic->ParryCameraShake, CounterMagic->ParryCameraShakeScale);
	}
	ImpactEndsAt = FPlatformTime::Seconds() + CounterMagic->ParrySlowDuration;
	ImpactTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
		[this](float) { return UpdateCounterImpact(FPlatformTime::Seconds()); }));
	UE_LOG(LogBattleCoordinator, Log, TEXT("Parry impact slow motion started: Duration=%.3f Factor=%.3f"),
		CounterMagic->ParrySlowDuration, CounterMagic->ParrySlowFactor);
	return true;
}

bool UMagicPresentationComponent::UpdateCounterImpact(const double RealTime)
{
	if (!bImpactHolding) { return false; }
	if (!TargetActor.IsValid() || !Mesh.IsValid()) { FinishAction(true); return false; }
	if (RealTime < ImpactEndsAt) { return true; }
	const int64 Id = ActiveId;
	AActor* Target = TargetActor.Get();
	UMagicActionData* Data = ActiveData;
	RestoreCounterImpact();
	if (auto* Defense = GetOwner()->FindComponentByClass<UDefensePresentationComponent>()) { Defense->StopPresentation(); }
	if (!BeginAction(Data, Id, Target, true))
	{
		GetWorld()->GetSubsystem<UBattleCoordinator>()->NotifyParryCounterFinished(Id, true);
	}
	return false;
}

void UMagicPresentationComponent::RestoreCounterImpact()
{
	if (ImpactTicker.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(ImpactTicker); ImpactTicker.Reset(); }
	if (ImpactSlowId && GetWorld())
	{
		if (auto* Slow = GetWorld()->GetSubsystem<UBattleSlowMotionSubsystem>()) { Slow->End(ImpactSlowId); }
	}
	ImpactSlowId = 0;
	if (bImpactHolding) { UE_LOG(LogBattleCoordinator, Log, TEXT("Parry impact slow motion restored")); }
	bImpactHolding = false;
	if (IsValid(ParryImpactFX)) { ParryImpactFX->DestroyComponent(); }
	ParryImpactFX = nullptr;
	if (ImpactCamera.IsValid() && ImpactShake) { ImpactCamera->StopCameraShake(ImpactShake, true); }
	ImpactShake = nullptr;
	ImpactCamera.Reset();
}

bool UMagicPresentationComponent::BeginMagicAction(const EBattlePlayerCommand Command, const int64 ActionId, AActor* Target)
{
	return BeginAction(Command == EBattlePlayerCommand::PowerStrike ? StrongMagic.Get() : BasicMagic.Get(), ActionId, Target, false);
}

bool UMagicPresentationComponent::BeginAction(UMagicActionData* Data, const int64 ActionId, AActor* Target, const bool bCounter)
{
	StopPresentation();
	ActiveData = Data;
	bCounterAction = bCounter;
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	Mesh = Character ? Character->GetMesh() : nullptr;
	if (!ActiveData || !ActiveData->AttackMontage || !ActiveData->ValidateProjectile() || !Mesh.IsValid() || !Mesh->GetAnimInstance()
		|| (ActiveData->ChargeMontage && (!FMath::IsFinite(ActiveData->ChargeDuration) || ActiveData->ChargeDuration <= 0.0f)))
	{
		StopPresentation();
		return false;
	}
	ActiveId = ActionId;
	TargetActor = Target;
	if (const auto* Battle=GetWorld()->GetSubsystem<UBattleCoordinator>();!bCounter && Battle && Battle->IsActiveActionAreaAttack())
	{
		bAreaAction=true;
		for (AActor* Member:Battle->GetActiveActionTargets()) { PresentationTargets.Add(Member); }
	}
	if (IsValid(Target))
	{
		FVector Facing = (bAreaAction?GetAimLocation():Target->GetActorLocation()) - GetOwner()->GetActorLocation();
		Facing.Z = 0.0f;
		if (!Facing.IsNearlyZero()) { GetOwner()->SetActorRotation(Facing.Rotation()); }
	}
	const bool bCharge = ActiveData->ChargeMontage != nullptr;
	PreparationDuration = ActiveData->GetPreparationDuration();
	bCastPreparing = !bCharge && !bCounter && ActiveData->bEnableTimingBonus;
	CastHoldPosition = FMath::Clamp(FMath::IsFinite(ActiveData->CastQTEHoldTime) ? ActiveData->CastQTEHoldTime : .8f,
		ActiveData->AttackStartTime, ActiveData->AttackMontage->GetPlayLength()-.01f);
	if (!PlayStage(bCharge ? ActiveData->ChargeMontage : ActiveData->AttackMontage, bCharge))
	{
		StopPresentation();
		return false;
	}
	if (bCharge || bCastPreparing)
	{
		GetWorld()->GetSubsystem<UBattleCoordinator>()->BeginPlayerPreparation(ActiveId, ActiveData->GetQTEDuration(),
			ActiveData->TimingSuccessDuration, ActiveData->TimingDamageMultiplier, ActiveData->bEnableTimingBonus, ActiveData->QTEPattern);
		FTimerDelegate Delegate = bCharge ? FTimerDelegate::CreateUObject(this, &ThisClass::FinishCharge, ActiveId)
			: FTimerDelegate::CreateUObject(this, &ThisClass::FinishCastPreparation, ActiveId);
		GetWorld()->GetTimerManager().SetTimer(ChargeTimer, Delegate, PreparationDuration, false);
		if (bCastPreparing)
		{
			GetWorld()->GetTimerManager().SetTimer(CastPauseTimer, FTimerDelegate::CreateUObject(this, &ThisClass::PauseCastForQTE, ActiveId),
				FMath::Max(.001f,(CastHoldPosition-ActiveData->AttackStartTime)/ActiveData->AttackPlayRate),false);
		}
	}
	return true;
}

bool UMagicPresentationComponent::PlayStage(UAnimMontage* Montage, const bool bCharge, float StartOverride)
{
	UAnimInstance* Anim = Mesh.IsValid() ? Mesh->GetAnimInstance() : nullptr;
	const float ChargeRate = Montage && bCharge ? FMath::Min(1.f, Montage->GetPlayLength() / PreparationDuration) : 1.f;
	if (!Anim || !Montage || Anim->Montage_Play(Montage, bCharge ? ChargeRate : ActiveData->AttackPlayRate,
		EMontagePlayReturnType::MontageLength, bCharge ? 0.0f : StartOverride>=0 ? StartOverride : ActiveData->AttackStartTime) <= 0.0f) { return false; }
	ActiveMontage = Montage;
	bCharging = bCharge;
	const FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage);
	ActiveMontageInstanceId = Instance ? Instance->GetInstanceID() : INDEX_NONE;
	FOnMontageEnded Delegate;
	Delegate.BindUObject(this, &ThisClass::OnMontageEnded, ActiveId);
	Anim->Montage_SetEndDelegate(Delegate, Montage);
	FOnMontageBlendingOutStarted BlendingOut;
	BlendingOut.BindUObject(this, &ThisClass::OnMontageBlendingOut, ActiveId);
	Anim->Montage_SetBlendingOutDelegate(BlendingOut, Montage);
	if (bCharge)
	{
		ChargeFX = SpawnEffect(ActiveData->ChargeEffect, ActiveData->ChargeEffectTransform, false);
		if (ChargeFX && ActiveData->bChargeEffectBetweenHands
			&& Mesh->DoesSocketExist(ActiveData->LeftHandSocket) && Mesh->DoesSocketExist(ActiveData->RightHandSocket))
		{
			ChargeFX->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			ChargeFX->AddTickPrerequisiteComponent(this);
			AddTickPrerequisiteComponent(Mesh.Get());
			UpdateChargeAnchor();
			SetComponentTickEnabled(true);
		}
	}
	else if (!ActiveData->bAttackEffectOnHit && !ActiveData->bUseProjectile)
	{
		SpawnAttackEffects();
		PlayAttackSound();
	}
	return true;
}

TArray<AActor*> UMagicPresentationComponent::GetEffectTargets() const
{
	TArray<AActor*> Result;
	if (!bAreaAction) { if (TargetActor.IsValid()) { Result.Add(TargetActor.Get()); } return Result; }
	for (const auto& Target:PresentationTargets)
	{
		if (Target.IsValid() && (bHitReported || FCombatTargetSelection::IsEligible(Target.Get()))) { Result.Add(Target.Get()); }
	}
	return Result;
}

FVector UMagicPresentationComponent::GetAimLocation() const
{
	const auto Targets=GetEffectTargets();
	FVector Center=FVector::ZeroVector;
	for (AActor* Target:Targets) { Center+=UCombatantVisualData::ResolveAnchor(Target,ECombatantAnchor::Aim); }
	return Targets.IsEmpty()?GetOwner()->GetActorLocation():Center/Targets.Num();
}

void UMagicPresentationComponent::SpawnAttackEffects()
{
	if (!ActiveData) { return; }
	for (AActor* Target:GetEffectTargets())
	{
		auto* Effect=SpawnEffect(ActiveData->AttackEffect,ActiveData->AttackEffectTransform,ActiveData->bAttackEffectAtTarget,Target);
		if (!AttackFX) { AttackFX=Effect; }
		else if (Effect) { AdditionalAttackFX.Add(Effect); }
	}
}

UNiagaraComponent* UMagicPresentationComponent::SpawnEffect(UNiagaraSystem* Effect, const FTransform& Transform, const bool bAtTarget,AActor* EffectTarget)
{
	if (!Effect || !Mesh.IsValid()) { return nullptr; }
	AActor* Target=EffectTarget?EffectTarget:TargetActor.Get();
	if (bCharging && ActiveData->bChargeEffectBetweenHands
		&& Mesh->DoesSocketExist(ActiveData->LeftHandSocket) && Mesh->DoesSocketExist(ActiveData->RightHandSocket))
	{
		const FVector Midpoint = (Mesh->GetSocketLocation(ActiveData->LeftHandSocket)
			+ Mesh->GetSocketLocation(ActiveData->RightHandSocket)) * 0.5f;
		const FTransform WorldTransform = Transform * FTransform(GetOwner()->GetActorRotation(), Midpoint);
		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Effect, WorldTransform.GetLocation(),
			WorldTransform.Rotator(), WorldTransform.GetScale3D());
	}
	if (!bCharging && !bAtTarget && ActiveData->bAttackEffectAimAtTarget && IsValid(Target))
	{
		const FVector SocketLocation = Mesh->GetSocketLocation(ActiveData->CastSocket);
		const FVector Forward = (Target->GetActorLocation() - SocketLocation).GetSafeNormal();
		const FTransform Origin(Forward.IsNearlyZero() ? GetOwner()->GetActorRotation() : Forward.Rotation(), SocketLocation);
		FTransform WorldTransform = Transform * Origin;
		// Aim from the final spawn point as well, so a configurable socket offset cannot miss the target.
		const FVector Direction = Target->GetActorLocation() - WorldTransform.GetLocation();
		if (!Direction.IsNearlyZero())
		{
			WorldTransform.SetRotation(Direction.Rotation().Quaternion() * Transform.GetRotation());
		}
		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Effect, WorldTransform.GetLocation(),
			WorldTransform.Rotator(), WorldTransform.GetScale3D());
	}
	if (ActiveData->bAttackEffectOnHit && !bAtTarget)
	{
		// A launched effect starts at the hand but must not rotate or move with the hand afterwards.
		const FTransform Origin(GetOwner()->GetActorRotation(), Mesh->GetSocketLocation(ActiveData->CastSocket));
		const FTransform WorldTransform = Transform * Origin;
		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Effect, WorldTransform.GetLocation(),
			WorldTransform.Rotator(), WorldTransform.GetScale3D());
	}
	if (bAtTarget && IsValid(Target))
	{
		const FTransform WorldTransform = Transform * Target->GetActorTransform();
		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Effect, WorldTransform.GetLocation(),
			WorldTransform.Rotator(), WorldTransform.GetScale3D());
	}
	return UNiagaraFunctionLibrary::SpawnSystemAttached(Effect, Mesh.Get(), ActiveData->CastSocket,
		Transform.GetLocation(), Transform.Rotator(), Transform.GetScale3D(),
		EAttachLocation::KeepRelativeOffset, true, ENCPoolMethod::None, true);
}

void UMagicPresentationComponent::PauseCastForQTE(int64 ExpectedId)
{
	if (!ActiveId || ExpectedId!=ActiveId || !bCastPreparing || !Mesh.IsValid() || !Mesh->GetAnimInstance()) { return; }
	UAnimInstance* Anim=Mesh->GetAnimInstance();
	if (Anim->Montage_IsActive(ActiveMontage))
	{
		Anim->Montage_SetPosition(ActiveMontage,CastHoldPosition);
		Anim->Montage_Pause(ActiveMontage); bCastPaused=true;
	}
}

void UMagicPresentationComponent::FinishCastPreparation(int64 ExpectedId)
{
	if (!ActiveId || ExpectedId!=ActiveId || !bCastPreparing) { return; }
	bCastPreparing=false;
	GetWorld()->GetTimerManager().ClearTimer(CastPauseTimer);
	GetWorld()->GetSubsystem<UBattleCoordinator>()->NotifyPlayerPreparationFinished(ActiveId);
	UAnimInstance* Anim=Mesh.IsValid() ? Mesh->GetAnimInstance() : nullptr;
	if (!Anim) { FinishAction(true); return; }
	if (Anim->Montage_IsActive(ActiveMontage))
	{
		if (bCastPaused) { Anim->Montage_Resume(ActiveMontage); }
	}
	else
	{
		if (!PlayStage(ActiveData->AttackMontage,false,CastHoldPosition)) { FinishAction(true); return; }
	}
	bCastPaused=false;
}

void UMagicPresentationComponent::FinishCharge(const int64 ExpectedId)
{
	if (ExpectedId != ActiveId || !bCharging) { return; }
	GetWorld()->GetSubsystem<UBattleCoordinator>()->NotifyPlayerPreparationFinished(ActiveId);
	SetComponentTickEnabled(false);
	GetWorld()->GetTimerManager().ClearTimer(ChargeTimer);
	if (ChargeFX) { ChargeFX->DestroyComponent(); ChargeFX = nullptr; }
	if (Mesh.IsValid() && Mesh->GetAnimInstance())
	{
		FOnMontageEnded Empty;
		Mesh->GetAnimInstance()->Montage_SetEndDelegate(Empty, ActiveMontage);
		FOnMontageBlendingOutStarted EmptyBlend;
		Mesh->GetAnimInstance()->Montage_SetBlendingOutDelegate(EmptyBlend, ActiveMontage);
		Mesh->GetAnimInstance()->Montage_Stop(0.15f, ActiveMontage);
	}
	if (!PlayStage(ActiveData->AttackMontage, false)) { FinishAction(true); }
}

void UMagicPresentationComponent::OnMontageEnded(UAnimMontage* Montage, const bool bInterrupted, const int64 ExpectedId)
{
	if (ExpectedId != ActiveId || Montage != ActiveMontage) { return; }
	if (!bInterrupted && bCastPreparing) { return; }
	// The preparation timer owns release; a short montage must not bypass the post-timing hold.
	if (!bInterrupted && bCharging) { return; }
	else if (!bInterrupted && ActiveData && ActiveData->bUseProjectile)
	{
		// 공격 모션과 발사체 비행이 모두 끝났을 때 턴을 넘긴다.
		bAttackMontageFinished = true;
		if (!ProjectileFlight.HasLaunched())
		{
			UE_LOG(LogBattleCoordinator, Warning, TEXT("Projectile launch marker missing; launching at normal montage completion."));
			if (!LaunchProjectile()) { FinishAction(true); return; }
		}
		if (ProjectileFlight.HasArrived()) { FinishAction(false); }
	}
	else { FinishAction(bInterrupted); }
}

void UMagicPresentationComponent::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted, int64 ExpectedId)
{
	if (bInterrupted && ExpectedId == ActiveId && Montage == ActiveMontage
		&& ActiveData && (bCharging || ActiveData->bUseProjectile)) { FinishAction(true); }
}

void UMagicPresentationComponent::HandleHitNotify(USkeletalMeshComponent* SourceMesh, const int32 MontageInstanceId)
{
	if (ActiveId == 0 || bCharging || SourceMesh != Mesh.Get() || MontageInstanceId != ActiveMontageInstanceId) { return; }
	if (bCastPreparing) { PauseCastForQTE(ActiveId); return; }
	if (ActiveData && ActiveData->bUseProjectile)
	{
		if (!ProjectileFlight.HasLaunched() && !LaunchProjectile()) { FinishAction(true); }
		return;
	}
	if (UBattleCoordinator* Battle = GetWorld()->GetSubsystem<UBattleCoordinator>())
	{
		if (ReportHit() && ActiveData && ActiveData->bAttackEffectOnHit)
		{
			SpawnAttackEffects();
		}
	}
}

bool UMagicPresentationComponent::ReportHit()
{
	UBattleCoordinator* Battle = GetWorld()->GetSubsystem<UBattleCoordinator>();
	if (!Battle) { return false; }
	if (!bCounterAction)
	{
		const auto HitTargets=GetEffectTargets();
		const bool bAccepted = Battle->NotifyPlayerActionHit(ActiveId);
		if (bAccepted)
		{
			if (bAreaAction)
			{
				PresentationTargets.Reset();
				for (AActor* Target:HitTargets) { PresentationTargets.Add(Target); }
			}
			bHitReported=true; PlayAttackSound(); PlayImpactSound();
		}
		return bAccepted;
	}
	if (!Battle->NotifyParryCounterHit(ActiveId)) { return false; }
	PlayImpactSound();
	return true;
}

void UMagicPresentationComponent::PlayAttackSound()
{
	if (bAttackSoundPlayed || !ActiveId || !ActiveData) { return; }
	bAttackSoundPlayed = true;
	CombatSoundPlayback::Play(GetOwner(), ActiveData->AttackSound, GetOwner()->GetActorLocation(),
		ActiveData->AttackSoundVolume, ActiveData->AttackSoundDelay, ActiveData->AttackSoundStartTime);
}

void UMagicPresentationComponent::PlayImpactSound()
{
	if (!ActiveData) { return; }
	const FVector Location = bAreaAction?GetAimLocation():ActiveData->bUseProjectile ? ProjectileFlight.GetEnd()
		: TargetActor.IsValid() ? TargetActor->GetActorLocation() : GetOwner()->GetActorLocation();
	CombatSoundPlayback::Play(GetOwner(), ActiveData->ImpactSound, Location,
		ActiveData->ImpactSoundVolume, ActiveData->ImpactSoundDelay, ActiveData->ImpactSoundStartTime);
}

void UMagicPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bCharging) { UpdateChargeAnchor(); }
	else if (ProjectileFlight.IsFlying())
	{
		// 발사한 프레임의 경과 시간에는 발사 전 시간이 포함되므로 다음 프레임부터 이동한다.
		if (ProjectileFlight.CanAdvance(GFrameCounter)) { UpdateProjectile(DeltaTime); }
	}
	else { SetComponentTickEnabled(false); }
}

bool UMagicPresentationComponent::LaunchProjectile()
{
	if (!ActiveId || ProjectileFlight.HasLaunched() || !ActiveData || !ActiveData->bUseProjectile
		|| !ActiveData->ValidateProjectile() || !Mesh.IsValid() || GetEffectTargets().IsEmpty()) { return false; }
	const FTransform LaunchOrigin(GetOwner()->GetActorRotation(), Mesh->GetSocketLocation(ActiveData->CastSocket));
	const FTransform LaunchTransform = ActiveData->AttackEffectTransform * LaunchOrigin;
	const FVector Start = LaunchTransform.GetLocation();
	const FVector End = GetAimLocation();
	const float Duration = ActiveData->GetFlightDuration(FVector::Distance(Start, End));
	if (!ProjectileFlight.Launch(Start, End, Duration, GFrameCounter)) { return false; }
	PlayAttackSound();
	PendingImpactTransform = ActiveData->ImpactEffectTransform;
	PendingImpactEffect = ActiveData->ImpactEffect;
	if (ActiveData->AttackEffect)
	{
		ProjectileFX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ActiveData->AttackEffect,
			Start, (ProjectileFlight.GetFacing().Quaternion() * ActiveData->AttackEffectTransform.GetRotation()).Rotator(),
			LaunchTransform.GetScale3D());
		if (IsValid(ProjectileFX)) { ProjectileFX->AddTickPrerequisiteComponent(this); }
	}
	SetComponentTickEnabled(true);
	UE_LOG(LogBattleCoordinator, Log, TEXT("Magic projectile launched: Action=%lld Flight=%.3f Start=%s End=%s"),
		ActiveId, Duration, *Start.ToString(), *End.ToString());
	return true;
}

void UMagicPresentationComponent::UpdateProjectile(float DeltaTime)
{
	if (!ActiveId || !ProjectileFlight.IsFlying()) { return; }
	if (GetEffectTargets().IsEmpty() || !Mesh.IsValid()) { FinishAction(true); return; }
	const FVector Position = ProjectileFlight.Advance(DeltaTime);
	if (IsValid(ProjectileFX)) { ProjectileFX->SetWorldLocation(Position); }
	if (!ProjectileFlight.HasArrived()) { return; }
	SetComponentTickEnabled(false);
	if (IsValid(ProjectileFX))
	{
		FinishingEffects.RemoveAll([](const TWeakObjectPtr<UNiagaraComponent>& Effect) { return !Effect.IsValid(); });
		FinishingEffects.Add(ProjectileFX);
		ReleaseAttackEffect(ProjectileFX);
		ProjectileFX = nullptr;
	}
	UBattleCoordinator* Battle = GetWorld()->GetSubsystem<UBattleCoordinator>();
	if (!Battle || !ReportHit()) { FinishAction(true); return; }
	if (PendingImpactEffect)
	{
		for (AActor* Target:GetEffectTargets())
		{
			const FVector Point=bAreaAction?UCombatantVisualData::ResolveAnchor(Target,ECombatantAnchor::Aim):ProjectileFlight.GetEnd();
			const FTransform Impact=PendingImpactTransform*FTransform(ProjectileFlight.GetFacing(),Point);
			auto* Effect=UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),PendingImpactEffect,Impact.GetLocation(),Impact.Rotator(),Impact.GetScale3D());
			if (!AttackFX) { AttackFX=Effect; } else if (Effect) { AdditionalAttackFX.Add(Effect); }
		}
	}
	UE_LOG(LogBattleCoordinator, Log, TEXT("Magic projectile arrived: Action=%lld Position=%s"), ActiveId, *ProjectileFlight.GetEnd().ToString());
	if (bAttackMontageFinished) { FinishAction(false); }
}

void UMagicPresentationComponent::UpdateChargeAnchor()
{
	if (!bCharging || !ActiveData || !IsValid(ChargeFX) || !Mesh.IsValid())
	{
		SetComponentTickEnabled(false);
		return;
	}
	const FVector Midpoint = (Mesh->GetSocketLocation(ActiveData->LeftHandSocket)
		+ Mesh->GetSocketLocation(ActiveData->RightHandSocket)) * 0.5f;
	const FTransform Origin(GetOwner()->GetActorRotation(), Midpoint);
	ChargeFX->SetWorldTransform(ActiveData->ChargeEffectTransform * Origin);
}

void UMagicPresentationComponent::FinishAction(const bool bInterrupted)
{
	if (const auto* Battle=GetWorld()->GetSubsystem<UBattleCoordinator>())
	{
		if (Battle->GetCurrentState()==EBattleState::Victory || Battle->GetCurrentState()==EBattleState::Defeat) { return; }
	}
	const int64 FinishedId = ActiveId;
	const bool bWasCounter = bCounterAction;
	if (!bInterrupted && ActiveData
		&& FMath::IsFinite(ActiveData->AttackEffectTailDuration) && ActiveData->AttackEffectTailDuration > 0.0f)
	{
		TArray<TObjectPtr<UNiagaraComponent>> Effects=AdditionalAttackFX;
		if (AttackFX) { Effects.Add(AttackFX); }
		for (UNiagaraComponent* Effect:Effects)
		{
		if (!IsValid(Effect)) { continue; }
		// Visual completion must not delay damage or the next turn. Keep this effect independent of the action.
		FinishingEffects.RemoveAll([](const TWeakObjectPtr<UNiagaraComponent>& Effect) { return !Effect.IsValid(); });
		FinishingEffects.Add(Effect);
		FTimerHandle TailTimer;
		GetWorld()->GetTimerManager().SetTimer(TailTimer,
			FTimerDelegate::CreateUObject(this, &ThisClass::ReleaseAttackEffect, TWeakObjectPtr<UNiagaraComponent>(Effect)),
			ActiveData->AttackEffectTailDuration, false);
		}
		AdditionalAttackFX.Reset();
		AttackFX = nullptr;
	}
	StopPresentation();
	if (UBattleCoordinator* Battle = GetWorld()->GetSubsystem<UBattleCoordinator>())
	{
		if (bWasCounter) { Battle->NotifyParryCounterFinished(FinishedId, bInterrupted); }
		else { Battle->NotifyPlayerActionFinished(FinishedId, bInterrupted); }
	}
}

void UMagicPresentationComponent::ReleaseAttackEffect(TWeakObjectPtr<UNiagaraComponent> Effect)
{
	if (Effect.IsValid())
	{
		Effect->SetAutoDestroy(true);
		Effect->Deactivate();
	}
}

void UMagicPresentationComponent::StopPresentation()
{
	PresentationTargets.Reset();bAreaAction=false;bHitReported=false;
	for (UNiagaraComponent* Effect:AdditionalAttackFX) { if (IsValid(Effect)) { Effect->DestroyComponent(); } }
	AdditionalAttackFX.Reset();
	bCastPreparing = false; bCastPaused = false;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(CastPauseTimer); }
	RestoreCounterImpact();
	SetComponentTickEnabled(false);
	if (Mesh.IsValid()) { RemoveTickPrerequisiteComponent(Mesh.Get()); }
	ActiveId = 0;
	bAttackSoundPlayed = false;
	bCounterAction = false;
	ActiveMontageInstanceId = INDEX_NONE;
	ProjectileFlight = FMagicProjectileFlight{};
	bAttackMontageFinished = false;
	PendingImpactEffect = nullptr;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(ChargeTimer); }
	if (Mesh.IsValid() && Mesh->GetAnimInstance() && ActiveMontage)
	{
		FOnMontageEnded Empty;
		Mesh->GetAnimInstance()->Montage_SetEndDelegate(Empty, ActiveMontage);
		FOnMontageBlendingOutStarted EmptyBlend;
		Mesh->GetAnimInstance()->Montage_SetBlendingOutDelegate(EmptyBlend, ActiveMontage);
		Mesh->GetAnimInstance()->Montage_Stop(0.15f, ActiveMontage);
	}
	if (IsValid(ChargeFX)) { ChargeFX->DestroyComponent(); }
	if (IsValid(AttackFX)) { AttackFX->DestroyComponent(); }
	ChargeFX = nullptr;
	AttackFX = nullptr;
	if (IsValid(ProjectileFX)) { ProjectileFX->DestroyComponent(); }
	ProjectileFX = nullptr;
	ActiveMontage = nullptr;
	ActiveData = nullptr;
	TargetActor.Reset();
	Mesh.Reset();
	bCharging = false;
}

void UMagicPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearAllTimersForObject(this); }
	for (const TWeakObjectPtr<UNiagaraComponent>& Effect : FinishingEffects)
	{
		if (Effect.IsValid()) { Effect->DestroyComponent(); }
	}
	FinishingEffects.Empty();
	StopPresentation();
	Super::EndPlay(Reason);
}
