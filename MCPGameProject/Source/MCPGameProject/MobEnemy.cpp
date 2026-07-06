// venom — Basic enemy mob. Chases the player. Future host for possession.

#include "MobEnemy.h"
#include "ParasitePawn.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor GoblinColor(0.12f, 0.55f, 0.15f);   // green
	const FLinearColor HighlightColor(1.0f, 0.85f, 0.1f);  // yellow
	const FLinearColor HitFlashColor(4.0f, 4.0f, 4.0f);    // bright white
}

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
	BodyMesh->SetRelativeScale3D(FVector(NormalScale));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeFinder.Object);
	}
}

void AMobEnemy::BeginPlay()
{
	Super::BeginPlay();

	BodyMID = BodyMesh->CreateDynamicMaterialInstance(0);
	RefreshColor();
}

void AMobEnemy::RefreshColor()
{
	if (!BodyMID)
	{
		return;
	}
	const FLinearColor C = bHighlighted ? HighlightColor : GoblinColor;
	// Cover common param names on the engine basic-shape material.
	BodyMID->SetVectorParameterValue(TEXT("Color"), C);
	BodyMID->SetVectorParameterValue(TEXT("BaseColor"), C);
}

void AMobEnemy::SetHighlighted(bool bInHighlighted)
{
	bHighlighted = bInHighlighted;
	BodyMesh->SetRelativeScale3D(FVector(bInHighlighted ? HighlightScale : NormalScale));
	RefreshColor();
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

	// Chase: head straight for the player (on their plane).
	FVector ToPlayer = Player->GetActorLocation() - MyLoc;
	ToPlayer.Z = 0.f;
	const FVector ChaseDir = ToPlayer.GetSafeNormal();

	// Separation: push away from nearby mobs so they spread instead of stacking.
	FVector Separation = FVector::ZeroVector;
	const float SepRadiusSq = SeparationRadius * SeparationRadius;
	for (TActorIterator<AMobEnemy> It(GetWorld()); It; ++It)
	{
		AMobEnemy* Other = *It;
		if (Other == this)
		{
			continue;
		}

		FVector Away = MyLoc - Other->GetActorLocation();
		Away.Z = 0.f;
		const float DistSq = Away.SizeSquared();
		if (DistSq > KINDA_SMALL_NUMBER && DistSq < SepRadiusSq)
		{
			// Closer neighbours push harder.
			const float Dist = FMath::Sqrt(DistSq);
			Separation += (Away / Dist) * (1.f - Dist / SeparationRadius);
		}
	}

	FVector Desired = ChaseDir + Separation * SeparationWeight;
	Desired.Z = 0.f;

	FVector Delta = FVector::ZeroVector;
	if (Desired.SizeSquared() > KINDA_SMALL_NUMBER)
	{
		Delta += Desired.GetSafeNormal() * MoveSpeed * DeltaSeconds;
	}
	// Knockback rides on top of the chase movement, then bleeds off.
	Delta += KnockbackVelocity * DeltaSeconds;
	if (!Delta.IsNearlyZero())
	{
		SetActorLocation(MyLoc + Delta, /*bSweep=*/false);
	}
	KnockbackVelocity = FMath::VInterpTo(KnockbackVelocity, FVector::ZeroVector, DeltaSeconds, KnockbackDecay);

	// Always face the player regardless of the separation/knockback nudge.
	if (!ChaseDir.IsNearlyZero())
	{
		SetActorRotation(ChaseDir.Rotation());
	}
}

void AMobEnemy::TakeHit(float DamageAmount)
{
	Health -= DamageAmount;
	if (Health <= 0.f)
	{
		if (AParasitePawn* Player = Cast<AParasitePawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			Player->AddXP(XPReward);
		}
		Destroy();
		return;
	}

	// Brief white flash for hit feedback.
	if (BodyMID)
	{
		BodyMID->SetVectorParameterValue(TEXT("Color"), HitFlashColor);
		BodyMID->SetVectorParameterValue(TEXT("BaseColor"), HitFlashColor);
		GetWorldTimerManager().SetTimer(FlashTimer, this, &AMobEnemy::RefreshColor, 0.08f, false);
	}
}

void AMobEnemy::ApplyKnockback(const FVector& Impulse)
{
	KnockbackVelocity += Impulse;
}
