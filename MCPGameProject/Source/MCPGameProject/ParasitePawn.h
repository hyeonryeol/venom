// venom — Parasite player pawn (top-down survivor)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ParasitePawn.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UFloatingPawnMovement;
class UInputAction;
class UInputMappingContext;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class AMobEnemy;
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

	// Currently highlighted candidate (raw UPROPERTY -> auto-nulled if it dies).
	UPROPERTY()
	AMobEnemy* SelectedTarget;

	// True once we're wearing a host body.
	UPROPERTY()
	bool bIsPossessing = false;

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

	FTimerHandle AttackTimer;

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
	float HostMaxHP = 100.f;

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

	// Form meshes (parasite sphere <-> host cube), resolved in the constructor.
	UPROPERTY()
	UStaticMesh* ParasiteMesh;

	UPROPERTY()
	UStaticMesh* HostMesh;

	UPROPERTY()
	UMaterialInterface* TintMaterial;

	UPROPERTY()
	UMaterialInstanceDynamic* BodyMID;

	// Tint the body for the current form (parasite vs host).
	void ApplyBodyColor();

	void MoveForward(const FInputActionValue& Value);
	void MoveBackward(const FInputActionValue& Value);
	void MoveLeft(const FInputActionValue& Value);
	void MoveRight(const FInputActionValue& Value);

	void SelectNextHost(const FInputActionValue& Value);
	void PerformPossess(const FInputActionValue& Value);

	void SetSelectedTarget(AMobEnemy* NewTarget);

	void PerformAttack();
	void LevelUp();

	void StartAugmentChoice();
	void ChooseAugment(int32 OptionIndex);
	void ApplyAugment(int32 AugmentId);
	FString AugmentName(int32 AugmentId) const;

	void OnAugment1(const FInputActionValue& Value);
	void OnAugment2(const FInputActionValue& Value);
	void OnAugment3(const FInputActionValue& Value);

	void EjectFromHost();
	void EndInvuln();
	void GameOver();
	void OnRestart(const FInputActionValue& Value);
};
