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

	/** Launch toward Direction; set speed and damage. */
	void Launch(const FVector& Direction, float Speed, float InDamage);

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

	float Damage = 10.f;
};
