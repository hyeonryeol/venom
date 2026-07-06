// venom — Parasite player pawn (top-down survivor)

#include "ParasitePawn.h"
#include "MobEnemy.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
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

#include "Materials/MaterialInstanceDynamic.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"

namespace
{
	const FLinearColor ParasiteColor(0.55f, 0.1f, 0.8f);  // purple (fallback tint)
}

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
		BodyMesh->SetStaticMesh(ParasiteMesh);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TintFinder(TEXT("/Game/Materials/M_VenomTint.M_VenomTint"));
	if (TintFinder.Succeeded())
	{
		TintMaterial = TintFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SymbioteFinder(TEXT("/Game/Materials/M_Symbiote.M_Symbiote"));
	if (SymbioteFinder.Succeeded())
	{
		SymbioteMaterial = SymbioteFinder.Object;
	}

	// Host form: the possessed goblin (same mesh as enemies, symbiote-tinted).
	HostGoblinMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HostGoblinMesh"));
	HostGoblinMesh->SetupAttachment(RootComponent);
	HostGoblinMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HostGoblinMesh->SetRelativeScale3D(FVector(1.3f)); // a bit bigger than enemy goblins
	HostGoblinMesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	HostGoblinMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	HostGoblinMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	HostGoblinMesh->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> GoblinFinder(TEXT("/Game/Goblin_low_for_bake.Goblin_low_for_bake"));
	if (GoblinFinder.Succeeded())
	{
		HostGoblinMesh->SetSkeletalMeshAsset(GoblinFinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UAnimSequence> HostWalkFinder(TEXT("/Game/Goblin_low_for_bake_Anim_Anim_Full_Walk.Goblin_low_for_bake_Anim_Anim_Full_Walk"));
	if (HostWalkFinder.Succeeded())
	{
		HostWalkAnim = HostWalkFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UAnimSequence> HostIdleFinder(TEXT("/Game/Goblin_low_for_bake_Anim_Anim_Full_Idle.Goblin_low_for_bake_Anim_Anim_Full_Idle"));
	if (HostIdleFinder.Succeeded())
	{
		HostIdleAnim = HostIdleFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UAnimSequence> HostAttackFinder(TEXT("/Game/Goblin_low_for_bake_Anim_Anim_Full_Attack_One_Handed.Goblin_low_for_bake_Anim_Anim_Full_Attack_One_Handed"));
	if (HostAttackFinder.Succeeded())
	{
		HostAttackAnim = HostAttackFinder.Object;
	}

	// Fixed top-down camera boom (closer than before).
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 800.f;
	SpringArm->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
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
	RestartAction = CreateDefaultSubobject<UInputAction>(TEXT("RestartAction"));
	RestartAction->ValueType = EInputActionValueType::Boolean;

	InputMapping->MapKey(MoveForwardAction, EKeys::W);
	InputMapping->MapKey(MoveBackwardAction, EKeys::S);
	InputMapping->MapKey(MoveLeftAction, EKeys::A);
	InputMapping->MapKey(MoveRightAction, EKeys::D);
	InputMapping->MapKey(SelectHostAction, EKeys::Q);
	InputMapping->MapKey(PossessAction, EKeys::SpaceBar);
	InputMapping->MapKey(Augment1Action, EKeys::One);
	InputMapping->MapKey(Augment2Action, EKeys::Two);
	InputMapping->MapKey(Augment3Action, EKeys::Three);
	InputMapping->MapKey(RestartAction, EKeys::R);

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

	// Start bare and fragile — you must grab a host fast.
	MaxHealth = ParasiteMaxHP;
	Health = ParasiteMaxHP;

	ApplyBodyColor();
}

void AParasitePawn::ApplyBodyColor()
{
	// Parasite ooze: black glossy symbiote with a pulsing red rim.
	UMaterialInterface* Base = SymbioteMaterial ? SymbioteMaterial : TintMaterial;
	if (Base)
	{
		BodyMID = UMaterialInstanceDynamic::Create(Base, this);
		BodyMesh->SetMaterial(0, BodyMID);
		if (!SymbioteMaterial)
		{
			BodyMID->SetVectorParameterValue(TEXT("Color"), ParasiteColor);
		}
	}
}

void AParasitePawn::EnterHostForm()
{
	BodyMesh->SetVisibility(false);
	HostGoblinMesh->SetVisibility(true);

	// Symbiote-tint every material slot of the possessed goblin.
	HostMIDs.Reset();
	if (SymbioteMaterial)
	{
		const int32 NumMats = HostGoblinMesh->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(SymbioteMaterial, this);
			HostGoblinMesh->SetMaterial(i, MID);
			HostMIDs.Add(MID);
		}
	}

	bHostWalking = false;
	if (HostIdleAnim)
	{
		HostGoblinMesh->PlayAnimation(HostIdleAnim, true);
	}
}

void AParasitePawn::ExitHostForm()
{
	HostGoblinMesh->SetVisibility(false);
	HostMIDs.Reset();
	BodyMesh->SetVisibility(true);
	SetActorRotation(FRotator::ZeroRotator);
	ApplyBodyColor();
}

void AParasitePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Remember where we're heading — that's where attacks fire.
	{
		FVector Vel = GetVelocity();
		Vel.Z = 0.f;
		if (Vel.SizeSquared() > 400.f)
		{
			AimDirection = Vel.GetSafeNormal();
		}
	}

	// Possession-range indicator: a flat red ring around the parasite.
	DrawDebugCircle(GetWorld(), GetActorLocation(), PossessRange, 48, FColor::Red,
		/*bPersistent=*/false, /*LifeTime=*/-1.f, /*DepthPriority=*/0, /*Thickness=*/4.f,
		/*YAxis=*/FVector(1.f, 0.f, 0.f), /*ZAxis=*/FVector(0.f, 1.f, 0.f), /*bDrawAxis=*/false);

	// Parasite = crawling ooze: flat puddle that stretches along its motion and
	// undulates, plus a pulsing red rim.
	if (!bIsPossessing)
	{
		const float T = GetWorld()->GetTimeSeconds();
		if (BodyMID)
		{
			BodyMID->SetScalarParameterValue(TEXT("Pulse"), 0.55f + 0.45f * FMath::Sin(T * 4.f));
		}

		const float Base = 0.8f;
		const float FlatZ = 0.5f; // squashed onto the ground like slime

		FVector Vel = GetVelocity();
		Vel.Z = 0.f;
		if (Vel.Size() > 15.f)
		{
			// Point the stretch axis along movement, undulate it (crawl).
			BodyMesh->SetWorldRotation(Vel.Rotation());
			const float Stretch = 0.18f + 0.14f * FMath::Sin(T * 9.f);
			BodyMesh->SetRelativeScale3D(FVector(Base * (1.f + Stretch), Base * (1.f - 0.5f * Stretch), Base * FlatZ));
		}
		else
		{
			// Idle: a gently breathing blob.
			const float B = 1.f + 0.06f * FMath::Sin(T * 3.f);
			BodyMesh->SetRelativeScale3D(FVector(Base * B, Base * B, Base * FlatZ * B));
		}
	}
	else
	{
		// Host goblin: face movement, switch walk/idle, and keep the red rim lit.
		const float T = GetWorld()->GetTimeSeconds();
		for (UMaterialInstanceDynamic* MID : HostMIDs)
		{
			if (MID)
			{
				MID->SetScalarParameterValue(TEXT("Pulse"), 0.6f + 0.4f * FMath::Sin(T * 4.f));
			}
		}

		FVector Vel = GetVelocity();
		Vel.Z = 0.f;
		const bool bWantWalk = Vel.Size() > 20.f;
		if (bHostAttacking)
		{
			// Turn to face the target mid-swing (backpedal-and-slash look).
			SetActorRotation(AttackFacing.Rotation());
		}
		else if (bWantWalk)
		{
			SetActorRotation(Vel.Rotation());
		}
		if (!bHostAttacking && bWantWalk != bHostWalking)
		{
			bHostWalking = bWantWalk;
			UAnimSequence* Anim = bWantWalk ? HostWalkAnim : HostIdleAnim;
			if (Anim)
			{
				HostGoblinMesh->PlayAnimation(Anim, true);
			}
		}
	}

	// Drop the selection if the target has drifted out of range.
	if (SelectedTarget && FVector::Dist2D(SelectedTarget->GetActorLocation(), GetActorLocation()) > PossessRange)
	{
		SetSelectedTarget(nullptr);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(4, 0.f, bInvulnerable ? FColor::Cyan : FColor::White,
			FString::Printf(TEXT("HP %.0f/%.0f    Lv %d    XP %.0f/%.0f    [%s]%s"),
				FMath::Max(0.f, Health), MaxHealth, Level, XP, XPToNext,
				bIsPossessing ? TEXT("GOBLIN") : TEXT("parasite"),
				bInvulnerable ? TEXT("  (invuln)") : TEXT("")));

		if (!bPossessReady)
		{
			const float CD = GetWorldTimerManager().GetTimerRemaining(PossessCDTimer);
			GEngine->AddOnScreenDebugMessage(7, 0.f, FColor::Orange,
				FString::Printf(TEXT("Possess CD: %.1fs"), FMath::Max(0.f, CD)));
		}
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
		EIC->BindAction(RestartAction, ETriggerEvent::Started, this, &AParasitePawn::OnRestart);
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

	if (!bPossessReady)
	{
		if (GEngine)
		{
			const float CD = GetWorldTimerManager().GetTimerRemaining(PossessCDTimer);
			GEngine->AddOnScreenDebugMessage(1, 1.2f, FColor::Orange,
				FString::Printf(TEXT("Possess on cooldown (%.1fs)"), FMath::Max(0.f, CD)));
		}
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

	bIsPossessing = true;
	EnterHostForm();

	// Inherit the host's current HP (possess a healthy one!).
	MaxHealth = Host->GetMaxHealth();
	Health = Host->GetHealth();

	// Start the possess cooldown.
	bPossessReady = false;
	GetWorldTimerManager().SetTimer(PossessCDTimer, this, &AParasitePawn::OnPossessReady, PossessCooldown, false);

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

	// Auto-target: aim the wedge at the nearest mob in range.
	AMobEnemy* Nearest = nullptr;
	float BestDistSq = Range * Range;
	for (TActorIterator<AMobEnemy> It(GetWorld()); It; ++It)
	{
		AMobEnemy* Mob = *It;
		const float DistSq = FVector::DistSquared2D(Mob->GetActorLocation(), MyLoc);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Nearest = Mob;
		}
	}
	if (!Nearest)
	{
		return; // nothing in range — don't swing
	}

	FVector Aim = Nearest->GetActorLocation() - MyLoc;
	Aim.Z = 0.f;
	Aim = Aim.GetSafeNormal();
	AttackFacing = Aim; // the host turns to face this while swinging
	const float CosThreshold = FMath::Cos(FMath::DegreesToRadians(AttackHalfAngleDeg));

	// Gather targets inside the wedge around the target (damage can destroy mobs).
	TArray<AMobEnemy*> Targets;
	for (TActorIterator<AMobEnemy> It(GetWorld()); It; ++It)
	{
		AMobEnemy* Mob = *It;
		FVector ToMob = Mob->GetActorLocation() - MyLoc;
		ToMob.Z = 0.f;
		if (ToMob.SizeSquared() > Range * Range)
		{
			continue;
		}
		if (FVector::DotProduct(ToMob.GetSafeNormal(), Aim) >= CosThreshold)
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

	// Host swings its weapon when it actually connects.
	if (bHost && Targets.Num() > 0 && HostAttackAnim && !bHostAttacking)
	{
		bHostAttacking = true;
		HostGoblinMesh->PlayAnimation(HostAttackAnim, false);
		GetWorldTimerManager().SetTimer(HostAttackTimer, this,
			&AParasitePawn::OnHostAttackDone, HostAttackAnim->GetPlayLength(), false);
	}

	// Feedback: draw the attack wedge.
	const FVector L = MyLoc + FVector(0.f, 0.f, 10.f);
	const FVector EdgeA = Aim.RotateAngleAxis(AttackHalfAngleDeg, FVector::UpVector);
	const FVector EdgeB = Aim.RotateAngleAxis(-AttackHalfAngleDeg, FVector::UpVector);
	const FColor C = bHost ? FColor::Orange : FColor::White;
	DrawDebugLine(GetWorld(), L, L + EdgeA * Range, C, false, AttackInterval * 0.5f, 0, 3.f);
	DrawDebugLine(GetWorld(), L, L + EdgeB * Range, C, false, AttackInterval * 0.5f, 0, 3.f);
	DrawDebugLine(GetWorld(), L, L + Aim * Range, C, false, AttackInterval * 0.5f, 0, 2.f);
}

void AParasitePawn::OnHostAttackDone()
{
	bHostAttacking = false;
	if (!bIsPossessing)
	{
		return;
	}
	FVector Vel = GetVelocity();
	Vel.Z = 0.f;
	bHostWalking = Vel.Size() > 20.f;
	UAnimSequence* Anim = bHostWalking ? HostWalkAnim : HostIdleAnim;
	if (Anim)
	{
		HostGoblinMesh->PlayAnimation(Anim, true);
	}
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

void AParasitePawn::ReceiveContactDamage(float Amount)
{
	if (bDead || bInvulnerable)
	{
		return;
	}

	Health -= Amount;
	if (Health <= 0.f)
	{
		if (bIsPossessing)
		{
			EjectFromHost();
		}
		else
		{
			GameOver();
		}
	}
}

void AParasitePawn::EjectFromHost()
{
	// Lose the body, burst back out as the bare parasite.
	bIsPossessing = false;
	BodyMesh->SetRelativeScale3D(FVector(0.8f));
	ExitHostForm();

	MaxHealth = ParasiteMaxHP;
	Health = ParasiteMaxHP;

	// Brief grace so you aren't instantly re-killed.
	bInvulnerable = true;
	GetWorldTimerManager().SetTimer(InvulnTimer, this, &AParasitePawn::EndInvuln, EjectInvulnTime, false);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(2, 2.5f, FColor::Red, TEXT("Host destroyed! Exposed - grab a new host!"));
	}
}

void AParasitePawn::EndInvuln()
{
	bInvulnerable = false;
}

void AParasitePawn::OnPossessReady()
{
	bPossessReady = true;
}

void AParasitePawn::GameOver()
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	Health = 0.f;
	UGameplayStatics::SetGamePaused(this, true);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(20, 999.f, FColor::Red, TEXT("=== GAME OVER ===   press R to restart"));
	}
}

void AParasitePawn::OnRestart(const FInputActionValue& Value)
{
	if (!bDead)
	{
		return;
	}
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}
