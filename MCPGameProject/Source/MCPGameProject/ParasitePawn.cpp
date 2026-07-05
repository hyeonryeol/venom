// venom — Parasite player pawn (top-down survivor)

#include "ParasitePawn.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Engine/LocalPlayer.h"
#include "UObject/ConstructorHelpers.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"

AParasitePawn::AParasitePawn()
{
	PrimaryActorTick.bCanEverTick = false;

	// Collision / root
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(40.f);
	CollisionComp->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = CollisionComp;

	// Visible body (engine basic sphere, ~100u -> scale to match collision)
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeScale3D(FVector(0.8f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(SphereMeshFinder.Object);
	}

	// Top-down camera boom
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 1200.f;
	SpringArm->SetRelativeRotation(FRotator(-65.f, 0.f, 0.f));
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritYaw = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 8.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

	// Movement (consumes AddMovementInput)
	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->MaxSpeed = 600.f;
	Movement->Acceleration = 4000.f;
	Movement->Deceleration = 4000.f;

	// Enhanced Input, built in code so no .uasset is required.
	// Four 1D axis actions (one key each) avoid needing negate/swizzle modifiers.
	InputMapping = CreateDefaultSubobject<UInputMappingContext>(TEXT("InputMapping"));

	MoveForwardAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveForwardAction"));
	MoveForwardAction->ValueType = EInputActionValueType::Axis1D;
	MoveBackwardAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveBackwardAction"));
	MoveBackwardAction->ValueType = EInputActionValueType::Axis1D;
	MoveLeftAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveLeftAction"));
	MoveLeftAction->ValueType = EInputActionValueType::Axis1D;
	MoveRightAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveRightAction"));
	MoveRightAction->ValueType = EInputActionValueType::Axis1D;

	InputMapping->MapKey(MoveForwardAction, EKeys::W);
	InputMapping->MapKey(MoveBackwardAction, EKeys::S);
	InputMapping->MapKey(MoveLeftAction, EKeys::A);
	InputMapping->MapKey(MoveRightAction, EKeys::D);

	// Parasite doesn't rotate with the camera/controller.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}

void AParasitePawn::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(InputMapping, 0);
		}
	}
}

void AParasitePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EIC->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &AParasitePawn::MoveForward);
		EIC->BindAction(MoveBackwardAction, ETriggerEvent::Triggered, this, &AParasitePawn::MoveBackward);
		EIC->BindAction(MoveLeftAction, ETriggerEvent::Triggered, this, &AParasitePawn::MoveLeft);
		EIC->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &AParasitePawn::MoveRight);
	}
}

void AParasitePawn::MoveForward(const FInputActionValue& Value)
{
	AddMovementInput(FVector::ForwardVector, Value.Get<float>());
}

void AParasitePawn::MoveBackward(const FInputActionValue& Value)
{
	AddMovementInput(FVector::ForwardVector, -Value.Get<float>());
}

void AParasitePawn::MoveLeft(const FInputActionValue& Value)
{
	AddMovementInput(FVector::RightVector, -Value.Get<float>());
}

void AParasitePawn::MoveRight(const FInputActionValue& Value)
{
	AddMovementInput(FVector::RightVector, Value.Get<float>());
}
