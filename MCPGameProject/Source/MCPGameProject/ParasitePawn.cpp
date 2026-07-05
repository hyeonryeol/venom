// venom — Parasite player pawn (top-down survivor)

#include "ParasitePawn.h"
#include "MobEnemy.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
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

	// Visible body — parasite form by default.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeScale3D(FVector(0.8f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded())
	{
		ParasiteMesh = SphereFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		HostMesh = CubeFinder.Object;
	}
	if (ParasiteMesh)
	{
		BodyMesh->SetStaticMesh(ParasiteMesh);
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

	// Movement
	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->MaxSpeed = 600.f;
	Movement->Acceleration = 4000.f;
	Movement->Deceleration = 4000.f;

	// Enhanced Input, built in code so no .uasset is required.
	InputMapping = CreateDefaultSubobject<UInputMappingContext>(TEXT("InputMapping"));

	MoveForwardAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveForwardAction"));
	MoveForwardAction->ValueType = EInputActionValueType::Axis1D;
	MoveBackwardAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveBackwardAction"));
	MoveBackwardAction->ValueType = EInputActionValueType::Axis1D;
	MoveLeftAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveLeftAction"));
	MoveLeftAction->ValueType = EInputActionValueType::Axis1D;
	MoveRightAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveRightAction"));
	MoveRightAction->ValueType = EInputActionValueType::Axis1D;

	SelectHostAction = CreateDefaultSubobject<UInputAction>(TEXT("SelectHostAction"));
	SelectHostAction->ValueType = EInputActionValueType::Boolean;
	PossessAction = CreateDefaultSubobject<UInputAction>(TEXT("PossessAction"));
	PossessAction->ValueType = EInputActionValueType::Boolean;

	InputMapping->MapKey(MoveForwardAction, EKeys::W);
	InputMapping->MapKey(MoveBackwardAction, EKeys::S);
	InputMapping->MapKey(MoveLeftAction, EKeys::A);
	InputMapping->MapKey(MoveRightAction, EKeys::D);
	InputMapping->MapKey(SelectHostAction, EKeys::Q);
	InputMapping->MapKey(PossessAction, EKeys::SpaceBar);

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

		// Fire once per press (Started), not every frame.
		EIC->BindAction(SelectHostAction, ETriggerEvent::Started, this, &AParasitePawn::SelectNextHost);
		EIC->BindAction(PossessAction, ETriggerEvent::Started, this, &AParasitePawn::PerformPossess);
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

void AParasitePawn::SelectNextHost(const FInputActionValue& Value)
{
	const FVector MyLoc = GetActorLocation();

	TArray<AMobEnemy*> InRange;
	for (TActorIterator<AMobEnemy> It(GetWorld()); It; ++It)
	{
		AMobEnemy* Mob = *It;
		if (FVector::Dist2D(Mob->GetActorLocation(), MyLoc) <= PossessRange)
		{
			InRange.Add(Mob);
		}
	}

	if (InRange.Num() == 0)
	{
		SetSelectedTarget(nullptr);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1, 1.5f, FColor::Orange, TEXT("No mob in range"));
		}
		return;
	}

	// Stable order by distance so Q cycles predictably.
	InRange.Sort([MyLoc](const AMobEnemy& A, const AMobEnemy& B)
	{
		return FVector::DistSquared(A.GetActorLocation(), MyLoc) < FVector::DistSquared(B.GetActorLocation(), MyLoc);
	});

	const int32 CurrentIdx = InRange.IndexOfByKey(SelectedTarget);
	const int32 NextIdx = (CurrentIdx == INDEX_NONE) ? 0 : (CurrentIdx + 1) % InRange.Num();
	SetSelectedTarget(InRange[NextIdx]);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 1.5f, FColor::Cyan,
			FString::Printf(TEXT("Target %d/%d  (Space to possess)"), NextIdx + 1, InRange.Num()));
	}
}

void AParasitePawn::PerformPossess(const FInputActionValue& Value)
{
	AMobEnemy* Host = SelectedTarget;

	// Convenience: if nothing selected, grab the nearest in range.
	if (!Host)
	{
		const FVector MyLoc = GetActorLocation();
		float BestDistSq = PossessRange * PossessRange;
		for (TActorIterator<AMobEnemy> It(GetWorld()); It; ++It)
		{
			AMobEnemy* Mob = *It;
			const float DistSq = FVector::DistSquared2D(Mob->GetActorLocation(), MyLoc);
			if (DistSq <= BestDistSq)
			{
				BestDistSq = DistSq;
				Host = Mob;
			}
		}
	}

	if (!Host)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1, 1.5f, FColor::Orange, TEXT("No host to possess"));
		}
		return;
	}

	// Wear the host: adopt its body, jump to its position, consume the mob.
	const FVector HostLoc = Host->GetActorLocation();
	SetActorLocation(FVector(HostLoc.X, HostLoc.Y, GetActorLocation().Z));

	if (HostMesh)
	{
		BodyMesh->SetStaticMesh(HostMesh);
		BodyMesh->SetRelativeScale3D(FVector(0.9f));
	}
	bIsPossessing = true;

	SetSelectedTarget(nullptr);
	Host->Destroy();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(2, 2.0f, FColor::Green, TEXT("Possessed a host!"));
	}
}

void AParasitePawn::SetSelectedTarget(AMobEnemy* NewTarget)
{
	if (SelectedTarget == NewTarget)
	{
		return;
	}
	if (SelectedTarget)
	{
		SelectedTarget->SetHighlighted(false);
	}
	SelectedTarget = NewTarget;
	if (SelectedTarget)
	{
		SelectedTarget->SetHighlighted(true);
	}
}
