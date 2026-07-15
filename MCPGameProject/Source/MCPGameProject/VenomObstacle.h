// venom — Arena obstacle: a stone pillar that blocks projectiles (cover).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VenomObstacle.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * A pillar scattered around the arena. Pawns walk freely past it (overlap-only,
 * no navmesh snags), but projectiles are stopped by it — so it reads as cover.
 * The player's shots ricochet off it with the RICOCHET augment.
 */
UCLASS()
class MCPGAMEPROJECT_API AVenomObstacle : public AActor
{
	GENERATED_BODY()

public:
	AVenomObstacle();

	/** Horizontal radius of the pillar surface (for projectile reflection). */
	float GetBlockRadius() const { return BlockRadius; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	UStaticMeshComponent* Mesh;

	UPROPERTY()
	UMaterialInterface* TintMaterial;

	UPROPERTY()
	UMaterialInstanceDynamic* MID;

	UPROPERTY(EditAnywhere, Category = "Obstacle")
	float BlockRadius = 90.f;
};
