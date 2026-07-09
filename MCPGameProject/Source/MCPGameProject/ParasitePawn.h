// venom — Parasite player pawn (top-down survivor)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ParasitePawn.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UFloatingPawnMovement;
class UInputAction;
class UInputMappingContext;
class UStaticMesh;
class UAnimSequence;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USoundBase;
class AMobEnemy;
class AVenomProjectile;
struct FInputActionValue;

/**
 * The player: a lone parasite. Weak on its own — press Q to pick a nearby mob,
 * Space to possess it and wear it as a host.
 */
UCLASS()
class MCPGAMEPROJECT_API AParasitePawn : public APawn
{
	GENERATED_BODY()

public:
	AParasitePawn();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Award experience; may trigger one or more level-ups. */
	void AddXP(float Amount);

	/** Damage from a mob touching us. Ejects (if hosting) or ends the game. */
	void ReceiveContactDamage(float Amount);

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	bool IsPossessing() const { return bIsPossessing; }
	bool IsInvulnerable() const { return bInvulnerable; }
	bool IsDead() const { return bDead; }

	// --- Augment picker API (used by the HUD card UI) ---
	bool IsChoosingAugment() const { return bChoosingAugment; }
	const TArray<int32>& GetAugmentOptions() const { return CurrentAugmentOptions; }
	FString AugmentTitle(int32 AugmentId) const;
	FString AugmentName(int32 AugmentId) const;
	void ChooseAugment(int32 OptionIndex);

protected:
	virtual void BeginPlay() override;

	// --- Components ---
	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	USphereComponent* CollisionComp;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	UStaticMeshComponent* BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	UCameraComponent* Camera;

	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	UFloatingPawnMovement* Movement;

	// Parasite form: the symbiote creature mesh (shown while a bare parasite).
	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	USkeletalMeshComponent* SymbioteMesh;

	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> SymbioteMIDs;

	// Host form: the possessed goblin, symbiote-tinted (hidden while a parasite).
	UPROPERTY(VisibleAnywhere, Category = "Parasite")
	USkeletalMeshComponent* HostGoblinMesh;

	// --- Input (created programmatically, no assets needed) ---
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputMappingContext* InputMapping;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MoveForwardAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MoveBackwardAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MoveLeftAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MoveRightAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* SelectHostAction; // Q

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* PossessAction; // Space

	// --- Possession ---
	// How far the parasite can reach to select/possess a host.
	UPROPERTY(EditAnywhere, Category = "Possession")
	float PossessRange = 450.f;

	// Cooldown between possessions (stops host-hopping to refill HP).
	UPROPERTY(EditAnywhere, Category = "Possession")
	float PossessCooldown = 10.f;

	bool bPossessReady = true;
	FTimerHandle PossessCDTimer;

	// Show the possess-range ring until this world time (set on a failed possess).
	float ShowRangeUntil = 0.f;

	// Currently highlighted candidate (raw UPROPERTY -> auto-nulled if it dies).
	UPROPERTY()
	AMobEnemy* SelectedTarget;

	// True once we're wearing a host body.
	UPROPERTY()
	bool bIsPossessing = false;

	// --- Cinematic possession leap (the symbiote pounces onto its host) ---
	bool bLeaping = false;

	UPROPERTY()
	AMobEnemy* LeapTarget = nullptr;

	float LeapElapsed = 0.f;
	FVector LeapStart = FVector::ZeroVector;

	// Brief "liquefy in place" windup before the pounce launches.
	UPROPERTY(EditAnywhere, Category = "Possession")
	float LeapWindup = 0.18f;

	// Airtime of the pounce and how high the arc rises.
	UPROPERTY(EditAnywhere, Category = "Possession")
	float LeapDuration = 0.28f;

	UPROPERTY(EditAnywhere, Category = "Possession")
	float LeapArcHeight = 170.f;

	// Brief "flood over the host" pop right after landing.
	bool bEnveloping = false;
	float EnvelopElapsed = 0.f;

	void StartLeap(AMobEnemy* Host);
	void LandOnHost(AMobEnemy* Host);

	// Swap between the humanoid symbiote and the liquid goo blob (BodyMesh).
	void EnterLiquidForm();
	void EndLiquidForm();

	// --- Attack (auto-pulse; stats depend on current form) ---
	UPROPERTY(EditAnywhere, Category = "Attack")
	float AttackInterval = 0.6f;

	// Parasite form: no damage, short reach, shoves mobs back.
	UPROPERTY(EditAnywhere, Category = "Attack")
	float ParasiteAttackRange = 130.f;

	UPROPERTY(EditAnywhere, Category = "Attack")
	float ParasiteKnockback = 900.f;

	// Host (goblin) form: real damage, longer reach, no knockback.
	UPROPERTY(EditAnywhere, Category = "Attack")
	float HostAttackRange = 300.f;

	UPROPERTY(EditAnywhere, Category = "Attack")
	float HostDamage = 25.f;

	// If the possessed host was a ranged goblin, our attack throws projectiles.
	bool bHostRanged = false;

	UPROPERTY()
	TSubclassOf<AVenomProjectile> HostProjectileClass;

	UPROPERTY(EditAnywhere, Category = "Attack")
	float HostProjectileSpeed = 1100.f;

	// Extra mobs the player's projectile passes through (augment).
	int32 PiercePower = 0;

	// Attacks hit within this half-angle of the facing direction (a wedge).
	UPROPERTY(EditAnywhere, Category = "Attack")
	float AttackHalfAngleDeg = 55.f;

	FTimerHandle AttackTimer;

	// Facing/aim direction (last movement direction).
	FVector AimDirection = FVector(1.f, 0.f, 0.f);

	// Direction of the current auto-targeted attack (toward nearest enemy).
	FVector AttackFacing = FVector(1.f, 0.f, 0.f);

	bool bHostAttacking = false;
	FTimerHandle HostAttackTimer;

	// --- Progression ---
	UPROPERTY()
	int32 Level = 1;

	UPROPERTY()
	float XP = 0.f;

	UPROPERTY()
	float XPToNext = 5.f;

	// --- Augment choice on level-up ---
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* Augment1Action; // 1

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* Augment2Action; // 2

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* Augment3Action; // 3

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* RestartAction; // R (on game over)

	bool bChoosingAugment = false;
	int32 PendingLevelUps = 0;
	TArray<int32> CurrentAugmentOptions;

	// --- Health / life ---
	UPROPERTY(EditAnywhere, Category = "Health")
	float ParasiteMaxHP = 30.f;

	// Grace window right after being ejected.
	UPROPERTY(EditAnywhere, Category = "Health")
	float EjectInvulnTime = 1.5f;

	float MaxHealth = 30.f;
	float Health = 30.f;
	bool bInvulnerable = false;
	bool bDead = false;
	FTimerHandle InvulnTimer;

	// --- Audio ---
	UPROPERTY() USoundBase* PossessSound;
	UPROPERTY() USoundBase* EjectSound;
	UPROPERTY() USoundBase* LevelUpSound;
	UPROPERTY() USoundBase* PickSound;
	UPROPERTY() USoundBase* HurtSound;
	UPROPERTY() USoundBase* GameOverSound;
	UPROPERTY() USoundBase* KnockbackSound;

	void PlayUISound(USoundBase* Sound);

	// --- Camera shake (procedural) ---
	float ShakeTimeLeft = 0.f;
	float ShakeDuration = 0.f;
	float ShakeAmplitude = 0.f;
	void AddCameraShake(float Amplitude, float Duration);

	// Parasite ooze mesh (sphere), resolved in the constructor.
	UPROPERTY()
	UStaticMesh* ParasiteMesh;

	UPROPERTY()
	UMaterialInterface* TintMaterial;

	// Black glossy symbiote look (pulsing red rim) — used by both forms.
	UPROPERTY()
	UMaterialInterface* SymbioteMaterial;

	UPROPERTY()
	UMaterialInstanceDynamic* BodyMID;

	// Parasite form: writhing goo tendrils around the core.
	UPROPERTY()
	TArray<UStaticMeshComponent*> Tendrils;

	void SetParasiteVisible(bool bVisible);

	// Host goblin animations + its symbiote-tinted material instances.
	UPROPERTY()
	UAnimSequence* HostWalkAnim;

	UPROPERTY()
	UAnimSequence* HostIdleAnim;

	UPROPERTY()
	UAnimSequence* HostAttackAnim;

	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> HostMIDs;

	bool bHostWalking = false;

	// Apply the symbiote look to the parasite ooze body.
	void ApplyBodyColor();

	// Show/dress the host goblin (symbiote-tinted) when possessing.
	void EnterHostForm();
	void ExitHostForm();

	void MoveForward(const FInputActionValue& Value);
	void MoveBackward(const FInputActionValue& Value);
	void MoveLeft(const FInputActionValue& Value);
	void MoveRight(const FInputActionValue& Value);

	void SelectNextHost(const FInputActionValue& Value);
	void PerformPossess(const FInputActionValue& Value);

	void SetSelectedTarget(AMobEnemy* NewTarget);

	void PerformAttack();
	void OnHostAttackDone();
	void LevelUp();

	void StartAugmentChoice();
	void ApplyAugment(int32 AugmentId);

	void OnAugment1(const FInputActionValue& Value);
	void OnAugment2(const FInputActionValue& Value);
	void OnAugment3(const FInputActionValue& Value);

	void EjectFromHost();
	void EndInvuln();
	void GameOver();
	void OnRestart(const FInputActionValue& Value);

	void OnPossessReady();
};
