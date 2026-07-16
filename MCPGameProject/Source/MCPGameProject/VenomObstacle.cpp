// venom — Arena obstacle: solid cover (pillar or boulder).

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
	Mesh->SetMobility(EComponentMobility::Movable); // spawned + reshaped at runtime

	// Pillar default: cylinder r50/h100 at scale 1 -> r90 / h350 here.
	Mesh->SetRelativeScale3D(FVector(1.8f, 1.8f, 3.5f));
	// Lift so the base sits slightly below the actor origin (ground plane).
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	Mesh->SetCastShadow(true);

	// Solid to pawns (player AND mobs), overlap for everything else so
	// projectiles still get their overlap event and can bounce/stop in code.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Overlap);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Mesh->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylFinder.Succeeded())
	{
		CylinderMesh = CylFinder.Object;
		Mesh->SetStaticMesh(CylinderMesh);
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded())
	{
		SphereMesh = SphereFinder.Object;
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

	if (bRock)
	{
		// Squat half-buried boulder: wide squashed sphere, lower profile.
		if (SphereMesh)
		{
			Mesh->SetStaticMesh(SphereMesh);
		}
		Mesh->SetRelativeScale3D(FVector(3.4f, 2.7f, 1.9f));
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f)); // sunk into the ground
		BlockRadius = 150.f;
	}

	if (TintMaterial && Mesh)
	{
		MID = UMaterialInstanceDynamic::Create(TintMaterial, this);
		Mesh->SetMaterial(0, MID);
		// Low values keep M_VenomTint's emissive term dim (reads as stone).
		MID->SetVectorParameterValue(TEXT("Color"),
			bRock ? FLinearColor(0.10f, 0.095f, 0.12f)   // lighter slate boulder
				  : FLinearColor(0.05f, 0.055f, 0.07f)); // dark pillar
	}
}
