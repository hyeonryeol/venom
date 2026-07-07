// venom — Ranged goblin: chases like a normal mob but shoots projectiles.

#include "MobRangedGoblin.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AMobRangedGoblin::AMobRangedGoblin()
{
	bRanged = true;
	MoveSpeed = 210.f;        // hangs a touch behind the melee goblins
	MaxHealth = 90.f;         // squishier
	Health = 90.f;
	XPReward = 2.f;

	// Slightly smaller so it reads as a scout/archer.
	MeshScale = 0.85f;
	if (BodyMesh)
	{
		BodyMesh->SetRelativeScale3D(FVector(MeshScale));
	}

	// Blue glow so ranged goblins are easy to spot (and pick to possess).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TintFinder(TEXT("/Game/Materials/M_RangedTint.M_RangedTint"));
	if (TintFinder.Succeeded())
	{
		BaseOverlay = TintFinder.Object;
	}
}
