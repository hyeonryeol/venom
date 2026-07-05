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
struct FInputActionValue;

/**
 * The player: a lone parasite. Weak on its own; the plan is to possess enemy
 * mobs as hosts. This class covers step 1 — top-down movement + camera.
 */
UCLASS()
class MCPGAMEPROJECT_API AParasitePawn : public APawn
{
	GENERATED_BODY()

public:
	AParasitePawn();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

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

	void MoveForward(const FInputActionValue& Value);
	void MoveBackward(const FInputActionValue& Value);
	void MoveLeft(const FInputActionValue& Value);
	void MoveRight(const FInputActionValue& Value);
};
