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

	// Form meshes (parasite sphere <-> host cube), resolved in the constructor.
	UPROPERTY()
	UStaticMesh* ParasiteMesh;

	UPROPERTY()
	UStaticMesh* HostMesh;

	void MoveForward(const FInputActionValue& Value);
	void MoveBackward(const FInputActionValue& Value);
	void MoveLeft(const FInputActionValue& Value);
	void MoveRight(const FInputActionValue& Value);

	void SelectNextHost(const FInputActionValue& Value);
	void PerformPossess(const FInputActionValue& Value);

	void SetSelectedTarget(AMobEnemy* NewTarget);
};
