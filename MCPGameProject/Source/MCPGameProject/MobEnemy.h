// venom — Basic enemy mob (the goblin). Chases the player; a possession host.

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

	/** Visual cue while this mob is the parasite's current possession target. */
	void SetHighlighted(bool bInHighlighted);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Mob")
	USphereComponent* CollisionComp;

	UPROPERTY(VisibleAnywhere, Category = "Mob")
	UStaticMeshComponent* BodyMesh;

	// Slower than the player (600) so the parasite can be kited.
	UPROPERTY(EditAnywhere, Category = "Mob")
	float MoveSpeed = 250.f;

	// Mobs within this distance push each other apart so they don't stack.
	UPROPERTY(EditAnywhere, Category = "Mob")
	float SeparationRadius = 110.f;

	// How strongly separation competes with the chase direction.
	UPROPERTY(EditAnywhere, Category = "Mob")
	float SeparationWeight = 1.5f;

	static constexpr float NormalScale = 0.9f;
	static constexpr float HighlightScale = 1.3f;
};
