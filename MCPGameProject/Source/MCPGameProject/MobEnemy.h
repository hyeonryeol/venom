// venom — Basic enemy mob (the goblin). Chases the player; a possession host.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MobEnemy.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class MCPGAMEPROJECT_API AMobEnemy : public APawn
{
	GENERATED_BODY()

public:
	AMobEnemy();

	virtual void Tick(float DeltaSeconds) override;

	/** Visual cue while this mob is the parasite's current possession target. */
	void SetHighlighted(bool bInHighlighted);

	/** Take damage; grants XP to the player and dies at <= 0 HP. */
	void TakeHit(float DamageAmount);

	/** Add an outward velocity impulse (knockback). */
	void ApplyKnockback(const FVector& Impulse);

protected:
	virtual void BeginPlay() override;

	// Re-applies the mob's colour based on current state (highlight/base).
	void RefreshColor();

	UPROPERTY()
	UMaterialInterface* TintMaterial;

	UPROPERTY()
	UMaterialInstanceDynamic* BodyMID;

	bool bHighlighted = false;
	FTimerHandle FlashTimer;

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

	UPROPERTY(EditAnywhere, Category = "Mob")
	float Health = 125.f; // 5 goblin-host hits at 25 damage

	UPROPERTY(EditAnywhere, Category = "Mob")
	float XPReward = 1.f;

	// Contact attack on the player.
	UPROPERTY(EditAnywhere, Category = "Mob")
	float ContactDamage = 7.f;

	UPROPERTY(EditAnywhere, Category = "Mob")
	float ContactRange = 110.f;

	UPROPERTY(EditAnywhere, Category = "Mob")
	float AttackCooldown = 0.7f;

	float LastAttackTime = -100.f;

	// How fast a knockback impulse bleeds off (higher = snappier).
	UPROPERTY(EditAnywhere, Category = "Mob")
	float KnockbackDecay = 5.f;

	FVector KnockbackVelocity = FVector::ZeroVector;

	static constexpr float NormalScale = 0.9f;
	static constexpr float HighlightScale = 1.3f;
};
