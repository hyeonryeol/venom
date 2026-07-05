// venom — Basic enemy mob. Chases the player. Future host for possession.

#include "MobEnemy.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AMobEnemy::AMobEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(45.f);
	// Overlap the player (for future possession) but don't block movement.
	CollisionComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RootComponent = CollisionComp;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeScale3D(FVector(0.9f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeFinder.Object);
	}
}

void AMobEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}

	const FVector MyLoc = GetActorLocation();
	FVector ToPlayer = Player->GetActorLocation() - MyLoc;
	ToPlayer.Z = 0.f; // move on the player's plane

	if (ToPlayer.SizeSquared() > 1.f)
	{
		const FVector Dir = ToPlayer.GetSafeNormal();
		// Direct move — no controller/movement-component needed. Slides over
		// terrain toward the player like a classic top-down survivor mob.
		SetActorLocation(MyLoc + Dir * MoveSpeed * DeltaSeconds, /*bSweep=*/false);
		SetActorRotation(Dir.Rotation());
	}
}
