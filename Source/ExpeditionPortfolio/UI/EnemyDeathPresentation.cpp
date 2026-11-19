#include "UI/EnemyDeathPresentation.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/CombatSoundPlayback.h"
#include "UI/EnemyPresentationData.h"

void FEnemyDeathPresentation::Capture(AActor& Actor)
{
	bWasHidden = Actor.IsHidden();
	bHadCollision = Actor.GetActorEnableCollision();
	bActive = true;
	Actor.SetActorEnableCollision(false);
}

float FEnemyDeathPresentation::PlayDeathPose(AActor& Actor, USkeletalMeshComponent* Mesh, const UEnemyPresentationData* Data)
{
	const auto Bounded = [](float Value, float Fallback, float Min, float Max)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, Min, Max) : Fallback;
	};
	float Lifetime = Bounded(Data ? Data->DeathFallbackDuration : .75f, .75f, .05f, 30.f);
	Montage = Data ? Data->Death.Get() : nullptr;
	if (UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr)
	{
		Anim->StopAllMontages(.05f);
		if (Montage)
		{
			const float Rate = Bounded(Data->DeathPlayRate, 1.f, .05f, 10.f);
			const float Played = Anim->Montage_Play(Montage, Rate, EMontagePlayReturnType::Duration);
			if (Played > 0 && FMath::IsFinite(Played))
			{
				Lifetime = Played;
				// 이 적의 마지막 자세를 유지하고 공유 몽타주 에셋의 설정은 보존한다.
				if (auto* Instance = Anim->GetActiveInstanceForMontage(Montage)) { Instance->bEnableAutoBlendOut = false; }
			}
		}
	}
	Lifetime += Bounded(Data ? Data->DeathDisappearDelay : .2f, .2f, 0.f, 30.f);
	if (Data)
	{
		const auto& Sound = Data->DeathSound;
		CombatSoundPlayback::Play(&Actor, Sound.Sound, Mesh ? Mesh->GetComponentLocation() : Actor.GetActorLocation(),
			Sound.Volume, Sound.Delay, Sound.StartTime);
	}
	UE_LOG(LogTemp, Log, TEXT("Enemy death started: Actor=%s Montage=%s FadeStartsAfter=%.2f"),
		*Actor.GetName(), *GetNameSafe(Montage), Lifetime);
	return Lifetime;
}

// 모든 슬롯에서 투명도를 조절할 수 있을 때만 이 적 전용 재질을 적용한다.
bool FEnemyDeathPresentation::BeginFade(USkeletalMeshComponent* Mesh, const UEnemyPresentationData* Data)
{
	const float Config = Data ? Data->DeathFadeDuration : 0.0f;
	Duration = FMath::IsFinite(Config) ? FMath::Clamp(Config, 0.f, 30.f) : .8f;
	FadeParameter = Data ? Data->DeathFadeParameter : NAME_None;
	if (Duration <= 0 || !Mesh || Mesh->GetNumMaterials() == 0 || FadeParameter.IsNone()) { return false; }

	TArray<UMaterialInterface*> Sources;
	for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
	{
		UMaterialInterface* Material = Data->DeathFadeMaterials.IsValidIndex(Index) && Data->DeathFadeMaterials[Index]
			? Data->DeathFadeMaterials[Index].Get() : Mesh->GetMaterial(Index);
		float Value = 1.0f;
		if (!Material || !Material->GetScalarParameterValue(FMaterialParameterInfo(FadeParameter), Value))
		{
			UE_LOG(LogTemp, Warning, TEXT("Enemy death fade unavailable: Actor=%s Slot=%d Parameter=%s. Configure a fade material in the enemy presentation DA."),
				*GetNameSafe(Mesh->GetOwner()), Index, *FadeParameter.ToString());
			return false;
		}
		Sources.Add(Material);
	}
	for (int32 Index = 0; Index < Sources.Num(); ++Index)
	{
		OriginalMaterials.Add(Mesh->GetMaterial(Index));
		auto* Instance = UMaterialInstanceDynamic::Create(Sources[Index], Mesh);
		if (!Instance) { RestoreMaterials(Mesh); return false; }
		Instance->SetScalarParameterValue(FadeParameter, 1.0f);
		FadeInstances.Add(Instance);
		Mesh->SetMaterial(Index, Instance);
	}
	Elapsed = 0.0f;
	bFading = true;
	UE_LOG(LogTemp, Log, TEXT("Enemy death fade started: Actor=%s Duration=%.2f"), *GetNameSafe(Mesh->GetOwner()), Duration);
	return true;
}

bool FEnemyDeathPresentation::AdvanceFade(float DeltaTime)
{
	if (!bFading) { return false; }
	Elapsed += FMath::Max(0.f, DeltaTime);
	const float Alpha = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
	const float Opacity = 1 - Alpha * Alpha * (3 - 2 * Alpha);
	for (UMaterialInstanceDynamic* Instance : FadeInstances)
	{
		if (Instance) { Instance->SetScalarParameterValue(FadeParameter, Opacity); }
	}
	return Alpha >= 1.0f;
}

void FEnemyDeathPresentation::Hide(AActor& Actor)
{
	bFading = false;
	Actor.SetActorHiddenInGame(true);
	UE_LOG(LogTemp, Log, TEXT("Enemy disappeared: Actor=%s"), *Actor.GetName());
}

void FEnemyDeathPresentation::RestoreMaterials(USkeletalMeshComponent* Mesh)
{
	if (Mesh)
	{
		for (int32 Index = 0; Index < OriginalMaterials.Num(); ++Index) { Mesh->SetMaterial(Index, OriginalMaterials[Index]); }
	}
	OriginalMaterials.Reset();
	FadeInstances.Reset();
}

// 재도전이나 후퇴로 HP가 돌아오면 처치 직전 재질과 표시·충돌 설정을 복원한다.
void FEnemyDeathPresentation::Restore(AActor* Actor, USkeletalMeshComponent* Mesh)
{
	RestoreMaterials(Mesh);
	if (Mesh && Mesh->GetAnimInstance() && Montage) { Mesh->GetAnimInstance()->Montage_Stop(0.f, Montage); }
	Montage = nullptr;
	bFading = false;
	bActive = false;
	if (Actor)
	{
		Actor->SetActorHiddenInGame(bWasHidden);
		Actor->SetActorEnableCollision(bHadCollision);
	}
}
