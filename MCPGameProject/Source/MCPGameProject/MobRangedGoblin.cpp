// venom — Ranged goblin: chases like a normal mob but shoots projectiles.

#include "MobRangedGoblin.h"
#include "Components/SkeletalMeshComponent.h"

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
}
