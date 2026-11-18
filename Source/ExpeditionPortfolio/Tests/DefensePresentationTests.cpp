#if WITH_DEV_AUTOMATION_TESTS
#include "UI/DefensePresentationComponent.h"
#include "UI/DefensePresentationData.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefensePresentationDataTest,
	"ExpeditionPortfolio.Combat.DefensePresentation.DataValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDefensePresentationDataTest::RunTest(const FString& Parameters)
{
	UDefensePresentationData* Data = NewObject<UDefensePresentationData>();
	FString Reason;
	TestTrue(TEXT("Default tuning valid even without optional animations"), Data->Validate(Reason));
	float* Positive[] = { &Data->ParryPlayRate, &Data->ParryDuration, &Data->DodgeRetreatDuration,
		&Data->DodgeReturnDuration, &Data->DodgePoseTime, &Data->HitReactionPlayRate };
	for (float* Field : Positive)
	{
		const float Saved = *Field;
		for (float Value : {0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
		{
			*Field = Value;
			TestFalse(TEXT("Unsafe duration or rate rejected"), Data->Validate(Reason));
		}
		*Field = Saved;
	}
	Data->DodgeDistance = -1.0f;
	TestFalse(TEXT("Negative retreat distance rejected"), Data->Validate(Reason));
	Data->DodgeDistance = 0.0f;
	TestTrue(TEXT("Zero distance is valid for pose-only tuning"), Data->Validate(Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefensePresentationRestoreTest,
	"ExpeditionPortfolio.Combat.DefensePresentation.DodgeRestoration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDefensePresentationRestoreTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AActor* Owner = World->SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(Owner);
	Owner->SetRootComponent(Root);
	Root->RegisterComponent();
	Owner->SetActorLocation(FVector(100, 200, 300));
	USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Owner);
	Mesh->SetupAttachment(Root);
	Mesh->RegisterComponent();
	const FTransform Original(FRotator(0, -90, 0), FVector(12, -8, -90), FVector(1.1f));
	Mesh->SetRelativeTransform(Original);
	UDefensePresentationComponent* Presentation = NewObject<UDefensePresentationComponent>(Owner);
	Presentation->RegisterComponent();
	Presentation->Mesh = Mesh;
	const FTransform ActorBefore = Owner->GetActorTransform();
	auto BeginVisualDodge = [&]()
	{
		Presentation->OriginalMeshTransform = Mesh->GetRelativeTransform();
		Presentation->DodgeHome = Owner->GetActorLocation();
		Presentation->DodgeMeshHome = Mesh->GetComponentLocation();
		Presentation->DodgeGroundPoints = {Presentation->DodgeHome, Presentation->DodgeHome + FVector(-140,0,0)};
		Presentation->RetreatDuration = 0.14f;
		Presentation->ReturnStartTime = 0.68f;
		Presentation->ReturnDuration = 0.22f;
		Presentation->DodgeElapsed = 0.0f;
		Presentation->bDodging = true;
		Presentation->SetComponentTickEnabled(true);
	};
	BeginVisualDodge();
	Presentation->UpdateDodge(0.07f);
	TestTrue(TEXT("Retreat interpolates rather than teleports"), FMath::IsNearlyEqual(Mesh->GetRelativeLocation().X, Original.GetLocation().X - 70));
	TestTrue(TEXT("Collision actor does not move"), ActorBefore.Equals(Owner->GetActorTransform()));
	Presentation->UpdateDodge(0.2f);
	TestTrue(TEXT("Hold remains at full retreat"), FMath::IsNearlyEqual(Mesh->GetRelativeLocation().X, Original.GetLocation().X - 140));
	Presentation->StopPresentation();
	TestTrue(TEXT("Interruption restores exact original transform"), Mesh->GetRelativeTransform().Equals(Original));
	TestFalse(TEXT("Interruption disables tick"), Presentation->IsComponentTickEnabled());
	BeginVisualDodge();
	Presentation->UpdateDodge(0.75f);
	TestTrue(TEXT("Return interpolates toward original position"), Mesh->GetRelativeLocation().X < Original.GetLocation().X && Mesh->GetRelativeLocation().X > Original.GetLocation().X - 140);
	Presentation->UpdateDodge(0.3f);
	TestTrue(TEXT("Completed repeated dodge restores original transform"), Mesh->GetRelativeTransform().Equals(Original));
	TestFalse(TEXT("Completion disables tick"), Presentation->IsComponentTickEnabled());
	TestTrue(TEXT("Repeated dodge still leaves actor unchanged"), ActorBefore.Equals(Owner->GetActorTransform()));
	World->DestroyWorld(false);
	return true;
}
#endif
