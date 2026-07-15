// venom — Arena obstacle: a stone pillar that blocks projectiles (cover).

#include "VenomObstacle.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AVenomObstacle::AVenomObstacle()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	// A tall pillar. Basic cylinder is r50 / h100 at scale 1 -> r90 / h350 here.
	Mesh->SetRelativeScale3D(FVector(1.8f, 1.8f, 3.5f));
	// Lift the mesh so its base sits near the actor origin (ground plane); the
	// collision then spans a little below the pawns up past projectile height.
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	Mesh->SetCastShadow(true);

	// Overlap-only: projectiles get an overlap event (and decide to bounce/stop);
	// pawns pass through so there are no navmesh/steering snags.
	Mesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylFinder.Succeeded())
	{
		Mesh->SetStaticMesh(CylFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TintFinder(TEXT("/Game/Materials/M_VenomTint.M_VenomTint"));
	if (TintFinder.Succeeded())
	{
		TintMaterial = TintFinder.Object;
	}
}

void AVenomObstacle::BeginPlay()
{
	Super::BeginPlay();

	if (TintMaterial && Mesh)
	{
		MID = UMaterialInstanceDynamic::Create(TintMaterial, this);
		Mesh->SetMaterial(0, MID);
		// Dark slate stone (low value keeps M_VenomTint's emissive term dim).
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.055f, 0.07f));
	}
}
