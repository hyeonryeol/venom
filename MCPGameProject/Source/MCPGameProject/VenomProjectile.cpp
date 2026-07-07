// venom — Enemy projectile fired by ranged goblins.

#include "VenomProjectile.h"
#include "ParasitePawn.h"

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
	CollisionComp->InitSphereRadius(22.f);
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
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(TintMaterial, this);
		Mesh->SetMaterial(0, MID);
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(4.0f, 0.4f, 0.05f)); // bright orange glow
	}
}

void AVenomProjectile::Launch(const FVector& Direction, float Speed, float InDamage)
{
	Damage = InDamage;
	const FVector Dir = Direction.GetSafeNormal();
	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = Speed;
	Movement->Velocity = Dir * Speed;
}

void AVenomProjectile::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Only hurt the player; pass through mobs and the shooter.
	if (AParasitePawn* Player = Cast<AParasitePawn>(OtherActor))
	{
		Player->ReceiveContactDamage(Damage);
		Destroy();
	}
}
