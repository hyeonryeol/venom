// venom — Basic enemy mob. Chases the player. Future host for possession.

#include "MobEnemy.h"
#include "ParasitePawn.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/PlayerCameraManager.h"
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
	UStaticMesh* CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	if (CubeMesh)
	{
		BodyMesh->SetStaticMesh(CubeMesh);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TintFinder(TEXT("/Game/Materials/M_VenomTint.M_VenomTint"));
	if (TintFinder.Succeeded())
	{
		TintMaterial = TintFinder.Object;
	}

	// Floating health bar: a background + fill made of thin cubes, above the mob.
	HealthBarRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HealthBarRoot"));
	HealthBarRoot->SetupAttachment(RootComponent);
	HealthBarRoot->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	HealthBarRoot->SetUsingAbsoluteRotation(true); // billboarded in Tick

	HealthBarBG = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HealthBarBG"));
	HealthBarBG->SetupAttachment(HealthBarRoot);
	HealthBarBG->SetStaticMesh(CubeMesh);
	HealthBarBG->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBarBG->SetCastShadow(false);
	HealthBarBG->SetRelativeScale3D(FVector(0.06f, 1.2f, 0.14f));

	HealthBarFill = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HealthBarFill"));
	HealthBarFill->SetupAttachment(HealthBarRoot);
	HealthBarFill->SetStaticMesh(CubeMesh);
	HealthBarFill->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBarFill->SetCastShadow(false);
	HealthBarFill->SetRelativeScale3D(FVector(0.07f, 1.2f, 0.15f));
	HealthBarFill->SetRelativeLocation(FVector(2.f, 0.f, 0.f));
}

void AMobEnemy::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;

	if (TintMaterial)
	{
		BodyMID = UMaterialInstanceDynamic::Create(TintMaterial, this);
		BodyMesh->SetMaterial(0, BodyMID);

		UMaterialInstanceDynamic* BgMID = UMaterialInstanceDynamic::Create(TintMaterial, this);
		HealthBarBG->SetMaterial(0, BgMID);
		BgMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.02f, 0.02f, 0.02f));

		FillMID = UMaterialInstanceDynamic::Create(TintMaterial, this);
		HealthBarFill->SetMaterial(0, FillMID);
	}
	RefreshColor();
	UpdateHealthBar();
}

void AMobEnemy::UpdateHealthBar()
{
	if (!HealthBarFill)
	{
		return;
	}
	const float Ratio = (MaxHealth > 0.f) ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f;
	const float FullWidth = 1.2f;                         // matches BG scale Y
	const float HalfWidthUnits = FullWidth * 100.f * 0.5f; // cube is 100 units

	HealthBarFill->SetRelativeScale3D(FVector(0.07f, FMath::Max(0.0001f, FullWidth * Ratio), 0.15f));
	HealthBarFill->SetRelativeLocation(FVector(2.f, -HalfWidthUnits * (1.f - Ratio), 0.f));

	if (FillMID)
	{
		const FLinearColor C = FMath::Lerp(FLinearColor(0.9f, 0.1f, 0.1f), FLinearColor(0.15f, 0.9f, 0.15f), Ratio);
		FillMID->SetVectorParameterValue(TEXT("Color"), C);
	}
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

	// Face the health bar toward the camera and refresh its fill.
	if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCam = Cam->GetCameraLocation() - HealthBarRoot->GetComponentLocation();
		if (!ToCam.IsNearlyZero())
		{
			HealthBarRoot->SetWorldRotation(ToCam.Rotation());
		}
	}
	UpdateHealthBar();

	// Contact attack: bite the player on a cooldown while touching.
	if (AParasitePawn* Parasite = Cast<AParasitePawn>(Player))
	{
		if (FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) <= ContactRange)
		{
			const float Now = GetWorld()->GetTimeSeconds();
			if (Now - LastAttackTime >= AttackCooldown)
			{
				LastAttackTime = Now;
				Parasite->ReceiveContactDamage(ContactDamage);
			}
		}
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
