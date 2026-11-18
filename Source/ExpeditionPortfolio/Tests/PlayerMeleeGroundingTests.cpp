#if WITH_DEV_AUTOMATION_TESTS
#include "UI/PlayerUtilityPresentationComponent.h"
#include "Combat/BattleGrounding.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerMeleeGroundingTest,"ExpeditionPortfolio.Combat.Movement.PlayerMeleeGrounding",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlayerMeleeGroundingTest::RunTest(const FString& Parameters)
{
	auto* World=UWorld::CreateWorld(EWorldType::Game,false);
	auto Box=[&](FVector Center,FVector Extent)
	{
		auto* Actor=World->SpawnActor<AActor>();auto* Shape=NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Shape);Shape->SetBoxExtent(Extent);
		Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Shape->SetCollisionObjectType(ECC_WorldStatic);
		Shape->SetCollisionResponseToAllChannels(ECR_Block);Shape->RegisterComponent();Actor->SetActorLocation(Center);
	};
	Box(FVector(0,0,-10),FVector(1000,500,10));
	Box(FVector(-300,0,20),FVector(300,500,20));
	auto* Player=World->SpawnActor<ACharacter>();Player->GetCapsuleComponent()->SetCapsuleSize(30,90);
	Player->SetActorLocation(FVector(400,0,92));
	auto* Mesh=Player->GetMesh();Mesh->SetRelativeLocation(FVector(0,0,-90));
	auto* Presentation=NewObject<UPlayerUtilityPresentationComponent>(Player);
	Player->AddInstanceComponent(Presentation);Presentation->RegisterComponent();
	const FVector Home=Player->GetActorLocation();const FTransform MeshHome=Mesh->GetRelativeTransform();
	Presentation->Mesh=Mesh;Presentation->OriginalTransform=MeshHome;Presentation->bMeshSaved=true;
	Presentation->TravelHome=Home;Presentation->TravelMeshHome=Mesh->GetComponentLocation();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(MeleeGroundTest),true,Player);FString Failure;
	TestTrue(TEXT("Player route climbs the step"),UBattleGroundingLibrary::BuildPath(World,Home,FVector(-400,0,92),FVector(30,30,90),FBattleGroundingSettings(),Query,Presentation->GroundTravelPoints,Failure));
	Presentation->ActiveId=1;Presentation->MoveDuration=1;Presentation->ApproachHeight=30;Presentation->ReturnHeight=18;
	Presentation->TickComponent(.75f,LEVELTICK_All,nullptr);
	TestTrue(TEXT("Approach adds raised pavement and authored leap"),FMath::IsNearlyEqual(Mesh->GetComponentLocation().Z,42.+30.*FMath::Sin(PI*.75),.1));
	TestEqual(TEXT("Encounter capsule remains at its anchor"),Player->GetActorLocation(),Home);
	Presentation->bReturning=true;Presentation->Elapsed=0;
	Presentation->TickComponent(.25f,LEVELTICK_All,nullptr);
	TestTrue(TEXT("Return starts above the raised pavement"),FMath::IsNearlyEqual(Mesh->GetComponentLocation().Z,42.+18.*FMath::Sin(PI*.25),.1));
	Presentation->TickComponent(.5f,LEVELTICK_All,nullptr);
	TestTrue(TEXT("Return descends to the lower road"),FMath::IsNearlyEqual(Mesh->GetComponentLocation().Z,2.+18.*FMath::Sin(PI*.75),.1));
	Presentation->StopPresentation();
	TestTrue(TEXT("Cancellation restores exact mesh transform"),Mesh->GetRelativeTransform().Equals(MeshHome));
	TestTrue(TEXT("Cancellation clears the cached path"),Presentation->GroundTravelPoints.IsEmpty());
	TestFalse(TEXT("No movement tick remains after cleanup"),Presentation->IsComponentTickEnabled());
	World->DestroyWorld(false);return true;
}
#endif
