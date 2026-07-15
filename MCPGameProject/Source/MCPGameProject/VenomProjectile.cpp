// venom — Enemy projectile fired by ranged goblins.

#include "VenomProjectile.h"
#include "ParasitePawn.h"
#include "MobEnemy.h"
#include "VenomObstacle.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AVenomProjectile::AVenomProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 3.5f;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(30.f);
	CollisionComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AVenomProjectile::OnOverlap);
	RootComponent = CollisionComp;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.45f));
	Mesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded())
	{
		Mesh->SetStaticMesh(SphereFinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TintFinder(TEXT("/Game/Materials/M_VenomTint.M_VenomTint"));
	if (TintFinder.Succeeded())
	{
		TintMaterial = TintFinder.Object;
	}

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->InitialSpeed = 900.f;
	Movement->MaxSpeed = 1400.f;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = false;
}

void AVenomProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (TintMaterial)
	{
		MID = UMaterialInstanceDynamic::Create(TintMaterial, this);
		Mesh->SetMaterial(0, MID);
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(4.0f, 0.4f, 0.05f)); // orange (enemy default)
	}
}

void AVenomProjectile::Launch(const FVector& Direction, float Speed, float InDamage, bool bHitMobs, int32 Pierce, int32 Bounce)
{
	Damage = InDamage;
	bDamagesMobs = bHitMobs;
	PierceRemaining = Pierce;
	BounceRemaining = Bounce;
	bLaunched = true; // only start hitting things once configured

	const FVector Dir = Direction.GetSafeNormal();
	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = Speed;
	Movement->Velocity = Dir * Speed;

	if (MID)
	{
		// Player shots glow venom-cyan; enemy shots stay orange.
		MID->SetVectorParameterValue(TEXT("Color"),
			bHitMobs ? FLinearColor(0.1f, 3.5f, 3.0f) : FLinearColor(4.0f, 0.4f, 0.05f));
	}
}

void AVenomProjectile::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Ignore the spawn-time overlap (fires before Launch sets friend/foe) and
	// any overlaps after we're spent.
	if (!bLaunched || bConsumed)
	{
		return;
	}

	// Obstacles are cover for everyone: stop the shot unless it can ricochet.
	if (AVenomObstacle* Obstacle = Cast<AVenomObstacle>(OtherActor))
	{
		if (BounceRemaining <= 0 || !Movement)
		{
			bConsumed = true;
			CollisionComp->SetGenerateOverlapEvents(false);
			if (Mesh)
			{
				Mesh->SetVisibility(false);
			}
			Destroy();
			return;
		}

		--BounceRemaining;

		// Reflect horizontally about the pillar's outward normal (center -> shot).
		FVector N = GetActorLocation() - Obstacle->GetActorLocation();
		N.Z = 0.f;
		N = N.GetSafeNormal();
		if (!N.IsNearlyZero())
		{
			const FVector V = Movement->Velocity;
			Movement->Velocity = V - 2.f * FVector::DotProduct(V, N) * N;
			// Nudge clear of the pillar so we don't re-enter the same overlap.
			AddActorWorldOffset(N * 12.f, false);
		}

		// After bouncing we may strike mobs we'd already brushed — let them count.
		HitActors.Reset();
		return;
	}

	if (bDamagesMobs)
	{
		// Player's shot: hurt mobs (with a slight knockback), pass through player.
		if (AMobEnemy* Mob = Cast<AMobEnemy>(OtherActor))
		{
			if (HitActors.Contains(Mob))
			{
				return; // don't hit the same mob twice while overlapping
			}
			HitActors.Add(Mob);

			FVector Push = Movement ? Movement->Velocity.GetSafeNormal() : GetActorForwardVector();
			Push.Z = 0.f;
			Mob->ApplyKnockback(Push.GetSafeNormal() * 350.f);
			Mob->TakeHit(Damage);

			if (PierceRemaining > 0)
			{
				--PierceRemaining; // keep flying through
			}
			else
			{
				bConsumed = true;
				CollisionComp->SetGenerateOverlapEvents(false);
				if (Mesh)
				{
					Mesh->SetVisibility(false);
				}
				Destroy();
			}
		}
	}
	else
	{
		// Enemy shot: hurt the player, pass through mobs.
		if (AParasitePawn* Player = Cast<AParasitePawn>(OtherActor))
		{
			Player->ReceiveContactDamage(Damage);
			bConsumed = true;
			Destroy();
		}
	}
}
