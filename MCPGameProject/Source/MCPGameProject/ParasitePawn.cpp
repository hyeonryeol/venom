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
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"

AParasitePawn::AParasitePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	// Keep drawing the augment overlay while the game is paused.
	PrimaryActorTick.bTickEvenWhenPaused = true;

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

	Augment1Action = CreateDefaultSubobject<UInputAction>(TEXT("Augment1Action"));
	Augment1Action->ValueType = EInputActionValueType::Boolean;
	Augment2Action = CreateDefaultSubobject<UInputAction>(TEXT("Augment2Action"));
	Augment2Action->ValueType = EInputActionValueType::Boolean;
	Augment3Action = CreateDefaultSubobject<UInputAction>(TEXT("Augment3Action"));
	Augment3Action->ValueType = EInputActionValueType::Boolean;

	InputMapping->MapKey(MoveForwardAction, EKeys::W);
	InputMapping->MapKey(MoveBackwardAction, EKeys::S);
	InputMapping->MapKey(MoveLeftAction, EKeys::A);
	InputMapping->MapKey(MoveRightAction, EKeys::D);
	InputMapping->MapKey(SelectHostAction, EKeys::Q);
	InputMapping->MapKey(PossessAction, EKeys::SpaceBar);
	InputMapping->MapKey(Augment1Action, EKeys::One);
	InputMapping->MapKey(Augment2Action, EKeys::Two);
	InputMapping->MapKey(Augment3Action, EKeys::Three);

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}

void AParasitePawn::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// (AVenomPlayerController already ticks input while paused, so the
		//  augment picker can read 1/2/3 during the pause.)
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(InputMapping, 0);
		}
	}

	// Auto-attack pulse.
	GetWorldTimerManager().SetTimer(AttackTimer, this, &AParasitePawn::PerformAttack, AttackInterval, true, AttackInterval);
}

void AParasitePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Possession-range indicator: a flat red ring around the parasite.
	DrawDebugCircle(GetWorld(), GetActorLocation(), PossessRange, 48, FColor::Red,
		/*bPersistent=*/false, /*LifeTime=*/-1.f, /*DepthPriority=*/0, /*Thickness=*/4.f,
		/*YAxis=*/FVector(1.f, 0.f, 0.f), /*ZAxis=*/FVector(0.f, 1.f, 0.f), /*bDrawAxis=*/false);

	// Drop the selection if the target has drifted out of range.
	if (SelectedTarget && FVector::Dist2D(SelectedTarget->GetActorLocation(), GetActorLocation()) > PossessRange)
	{
		SetSelectedTarget(nullptr);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(4, 0.f, FColor::White,
			FString::Printf(TEXT("Lv %d    XP %.0f/%.0f    [%s]"),
				Level, XP, XPToNext, bIsPossessing ? TEXT("GOBLIN") : TEXT("parasite")));
	}

	// Augment picker overlay.
	if (bChoosingAugment && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(10, 0.f, FColor::Magenta, TEXT("LEVEL UP! Pick an augment (press 1 / 2 / 3):"));
		for (int32 i = 0; i < CurrentAugmentOptions.Num(); ++i)
		{
			GEngine->AddOnScreenDebugMessage(11 + i, 0.f, FColor::Cyan,
				FString::Printf(TEXT("   [%d] %s"), i + 1, *AugmentName(CurrentAugmentOptions[i])));
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

		EIC->BindAction(Augment1Action, ETriggerEvent::Started, this, &AParasitePawn::OnAugment1);
		EIC->BindAction(Augment2Action, ETriggerEvent::Started, this, &AParasitePawn::OnAugment2);
		EIC->BindAction(Augment3Action, ETriggerEvent::Started, this, &AParasitePawn::OnAugment3);
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
	if (bChoosingAugment)
	{
		return;
	}

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
	if (bChoosingAugment)
	{
		return;
	}

	AMobEnemy* Host = SelectedTarget;

	// The selected target must still be inside the possess range.
	if (Host && FVector::Dist2D(Host->GetActorLocation(), GetActorLocation()) > PossessRange)
	{
		Host = nullptr;
	}

	// Convenience: if nothing valid selected, grab the nearest in range.
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

void AParasitePawn::PerformAttack()
{
	// Stats depend on the current form.
	const bool bHost = bIsPossessing;
	const float Range = bHost ? HostAttackRange : ParasiteAttackRange;
	const float Damage = bHost ? HostDamage : 0.f;
	const float Knockback = bHost ? 0.f : ParasiteKnockback;

	const FVector MyLoc = GetActorLocation();

	// Gather first, then apply (damage can destroy mobs).
	TArray<AMobEnemy*> Targets;
	for (TActorIterator<AMobEnemy> It(GetWorld()); It; ++It)
	{
		AMobEnemy* Mob = *It;
		if (FVector::DistSquared2D(Mob->GetActorLocation(), MyLoc) <= Range * Range)
		{
			Targets.Add(Mob);
		}
	}

	for (AMobEnemy* Mob : Targets)
	{
		if (Knockback > 0.f)
		{
			FVector Away = Mob->GetActorLocation() - MyLoc;
			Away.Z = 0.f;
			Mob->ApplyKnockback(Away.GetSafeNormal() * Knockback);
		}
		if (Damage > 0.f)
		{
			Mob->TakeHit(Damage);
		}
	}

	// Brief attack-range flash for feedback.
	DrawDebugCircle(GetWorld(), MyLoc, Range, 32, bHost ? FColor::Orange : FColor::White,
		false, AttackInterval * 0.5f, 0, 3.f, FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);
}

void AParasitePawn::AddXP(float Amount)
{
	XP += Amount;
	while (XP >= XPToNext)
	{
		XP -= XPToNext;
		LevelUp();
	}
}

void AParasitePawn::LevelUp()
{
	++Level;
	XPToNext = FMath::CeilToFloat(XPToNext * 1.3f);
	++PendingLevelUps;

	if (!bChoosingAugment)
	{
		StartAugmentChoice();
	}
}

void AParasitePawn::StartAugmentChoice()
{
	if (PendingLevelUps <= 0)
	{
		return;
	}

	// Offer 3 distinct augments from the pool (ids 0..4).
	TArray<int32> Pool = { 0, 1, 2, 3, 4 };
	CurrentAugmentOptions.Reset();
	for (int32 i = 0; i < 3 && Pool.Num() > 0; ++i)
	{
		const int32 Pick = FMath::RandRange(0, Pool.Num() - 1);
		CurrentAugmentOptions.Add(Pool[Pick]);
		Pool.RemoveAt(Pick);
	}

	bChoosingAugment = true;
	// Full pause while choosing. Input still reaches us because the controller
	// is set to tick when paused (see BeginPlay) and this pawn ticks-when-paused.
	UGameplayStatics::SetGamePaused(this, true);
}

void AParasitePawn::ChooseAugment(int32 OptionIndex)
{
	if (!bChoosingAugment || !CurrentAugmentOptions.IsValidIndex(OptionIndex))
	{
		return;
	}

	ApplyAugment(CurrentAugmentOptions[OptionIndex]);

	bChoosingAugment = false;
	--PendingLevelUps;

	if (PendingLevelUps > 0)
	{
		StartAugmentChoice(); // queue the next choice, stay paused
	}
	else
	{
		UGameplayStatics::SetGamePaused(this, false);
	}
}

void AParasitePawn::ApplyAugment(int32 AugmentId)
{
	switch (AugmentId)
	{
	case 0: // more host damage
		HostDamage += 10.f;
		break;
	case 1: // faster attacks
		AttackInterval = FMath::Max(0.15f, AttackInterval * 0.85f);
		GetWorldTimerManager().SetTimer(AttackTimer, this, &AParasitePawn::PerformAttack, AttackInterval, true, AttackInterval);
		break;
	case 2: // move speed
		if (Movement)
		{
			Movement->MaxSpeed += 100.f;
		}
		break;
	case 3: // possess range
		PossessRange += 100.f;
		break;
	case 4: // parasite knockback
		ParasiteKnockback += 400.f;
		break;
	default:
		break;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(6, 2.5f, FColor::Green,
			FString::Printf(TEXT("Augment: %s"), *AugmentName(AugmentId)));
	}
}

FString AParasitePawn::AugmentName(int32 AugmentId) const
{
	switch (AugmentId)
	{
	case 0: return TEXT("+10 Host Damage");
	case 1: return TEXT("+15% Attack Speed");
	case 2: return TEXT("+100 Move Speed");
	case 3: return TEXT("+100 Possess Range");
	case 4: return TEXT("+400 Parasite Knockback");
	default: return TEXT("Unknown");
	}
}

void AParasitePawn::OnAugment1(const FInputActionValue& Value) { ChooseAugment(0); }
void AParasitePawn::OnAugment2(const FInputActionValue& Value) { ChooseAugment(1); }
void AParasitePawn::OnAugment3(const FInputActionValue& Value) { ChooseAugment(2); }
