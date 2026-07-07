// venom — Enemy goblin mob. Chases the player; a possession host.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MobEnemy.generated.h"

class USphereComponent;
class USkeletalMeshComponent;
class UAnimSequence;
class USoundBase;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class AVenomProjectile;

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

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	bool IsRanged() const { return bRanged; }

protected:
	virtual void BeginPlay() override;

	void PlayLoop(UAnimSequence* Anim);
	void OnAttackAnimDone();
	void Die();
	void ClearHitFlash();

	UPROPERTY()
	USphereComponent* CollisionComp;

	UPROPERTY()
	USkeletalMeshComponent* BodyMesh;

	UPROPERTY()
	UAnimSequence* WalkAnim;

	UPROPERTY()
	UAnimSequence* AttackAnim;

	UPROPERTY()
	UAnimSequence* DeathAnim;

	// SFX + hit flash
	UPROPERTY()
	USoundBase* HitSound;

	UPROPERTY()
	USoundBase* DeathSound;

	UPROPERTY()
	UMaterialInterface* HitFlashOverlay;

	FTimerHandle HitFlashTimer;

	// Transform of the mesh relative to the collision sphere (tunable).
	UPROPERTY(EditAnywhere, Category = "Mob")
	float MeshScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Mob")
	float MeshZOffset = -90.f;

	UPROPERTY(EditAnywhere, Category = "Mob")
	float MeshYaw = -90.f;

	// Slower than the player (600) so the parasite can be kited.
	UPROPERTY(EditAnywhere, Category = "Mob")
	float MoveSpeed = 250.f;

	UPROPERTY(EditAnywhere, Category = "Mob")
	float SeparationRadius = 110.f;

	UPROPERTY(EditAnywhere, Category = "Mob")
	float SeparationWeight = 1.5f;

	// Mobs never get closer than this to the player (no overlapping the body).
	UPROPERTY(EditAnywhere, Category = "Mob")
	float MinPlayerDistance = 100.f;

	UPROPERTY(EditAnywhere, Category = "Mob")
	float MaxHealth = 125.f; // 5 goblin-host hits at 25 damage

	float Health = 125.f;

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

	// --- Ranged variant (fires projectiles instead of only melee) ---
	UPROPERTY(EditAnywhere, Category = "Ranged")
	bool bRanged = false;

	UPROPERTY(EditAnywhere, Category = "Ranged")
	float ShootRange = 750.f;

	UPROPERTY(EditAnywhere, Category = "Ranged")
	float ShootCooldown = 1.7f;

	UPROPERTY(EditAnywhere, Category = "Ranged")
	float ProjectileDamage = 9.f;

	UPROPERTY(EditAnywhere, Category = "Ranged")
	float ProjectileSpeed = 600.f;

	UPROPERTY(EditAnywhere, Category = "Ranged")
	TSubclassOf<AVenomProjectile> ProjectileClass;

	float LastShootTime = -100.f;

	void Shoot(const FVector& TargetLoc);

	// How fast a knockback impulse bleeds off (higher = snappier).
	UPROPERTY(EditAnywhere, Category = "Mob")
	float KnockbackDecay = 5.f;

	FVector KnockbackVelocity = FVector::ZeroVector;

	bool bDying = false;
	bool bAttacking = false;
	FTimerHandle AttackAnimTimer;
	FTimerHandle DeathTimer;
};
