// venom — Enemy projectile fired by ranged goblins.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VenomProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UMaterialInterface;

UCLASS()
class MCPGAMEPROJECT_API AVenomProjectile : public AActor
{
	GENERATED_BODY()

public:
	AVenomProjectile();

	/** Launch toward Direction. bHitMobs=true -> player's shot (damages mobs);
	 *  false -> enemy shot (damages the player). Pierce = extra mobs it passes
	 *  through (0 = destroyed on first hit). */
	void Launch(const FVector& Direction, float Speed, float InDamage, bool bHitMobs, int32 Pierce = 0);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY()
	USphereComponent* CollisionComp;

	UPROPERTY()
	UStaticMeshComponent* Mesh;

	UPROPERTY()
	UProjectileMovementComponent* Movement;

	UPROPERTY()
	UMaterialInterface* TintMaterial;

	UPROPERTY()
	UMaterialInstanceDynamic* MID;

	float Damage = 10.f;
	bool bDamagesMobs = false;
	bool bConsumed = false;
	bool bLaunched = false;
	int32 PierceRemaining = 0;

	UPROPERTY()
	TArray<AActor*> HitActors;
};
