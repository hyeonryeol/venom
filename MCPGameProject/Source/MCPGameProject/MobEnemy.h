// venom — Basic enemy mob. Chases the player. Future host for possession.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MobEnemy.generated.h"

class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class MCPGAMEPROJECT_API AMobEnemy : public APawn
{
	GENERATED_BODY()

public:
	AMobEnemy();

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Mob")
	USphereComponent* CollisionComp;

	UPROPERTY(VisibleAnywhere, Category = "Mob")
	UStaticMeshComponent* BodyMesh;

	// Slower than the player (600) so the parasite can be kited.
	UPROPERTY(EditAnywhere, Category = "Mob")
	float MoveSpeed = 250.f;
};
