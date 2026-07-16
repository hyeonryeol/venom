// venom — Arena obstacle: solid cover (pillar or boulder).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VenomObstacle.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * Solid cover scattered around the arena. Blocks pawns (player and mobs) and
 * stops projectiles; the player's shots ricochet off it with the RICOCHET
 * augment. Comes in two looks: a tall pillar (default) or a squat boulder
 * (SetRock before FinishSpawning).
 */
UCLASS()
class MCPGAMEPROJECT_API AVenomObstacle : public AActor
{
	GENERATED_BODY()

public:
	AVenomObstacle();

	/** Turn this pillar into a half-buried boulder (call before FinishSpawning). */
	void SetRock(bool bInRock) { bRock = bInRock; }

	/** Horizontal radius of the blocking surface (for projectile reflection). */
	float GetBlockRadius() const { return BlockRadius; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	UStaticMeshComponent* Mesh;

	UPROPERTY()
	UMaterialInterface* TintMaterial;

	UPROPERTY()
	UMaterialInstanceDynamic* MID;

	UPROPERTY()
	UStaticMesh* CylinderMesh;

	UPROPERTY()
	UStaticMesh* SphereMesh;

	bool bRock = false;

	UPROPERTY(EditAnywhere, Category = "Obstacle")
	float BlockRadius = 90.f;
};
