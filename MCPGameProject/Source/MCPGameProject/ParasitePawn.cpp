// venom — Parasite player pawn (top-down survivor)

#include "ParasitePawn.h"
#include "MobEnemy.h"
#include "VenomProjectile.h"
#include "GoblinAnimInstance.h"

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
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"

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
	BodyMesh->SetVisibility(false); // replaced by the symbiote skeletal mesh

	// Goo lumps: small spheres that cluster around the main blob (fake metaball)
	// and trail behind while leaping. Hidden until a possession leap.
	GooBlobs.Reset();
	for (int32 i = 0; i < NumGooBlobs; ++i)
	{
		UStaticMeshComponent* Blob = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("GooBlob%d"), i));
		Blob->SetupAttachment(RootComponent);
		Blob->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Blob->SetCastShadow(false);
		if (ParasiteMesh)
		{
			Blob->SetStaticMesh(ParasiteMesh);
		}
		Blob->SetRelativeScale3D(FVector(0.35f));
		Blob->SetVisibility(false);
		GooBlobs.Add(Blob);
	}

	// Parasite form: the symbiote creature (its own skeleton, no anims — posed).
	SymbioteMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SymbioteMesh"));
	SymbioteMesh->SetupAttachment(RootComponent);
	SymbioteMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SymbioteMesh->SetRelativeScale3D(FVector(60.f)); // model imported in metres (~2u)
	SymbioteMesh->SetRelativeLocation(FVector(0.f, 0.f, -40.f));
	SymbioteMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	SymbioteMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> SymbioteMeshFinder(TEXT("/Game/baisegongshengti_battle.baisegongshengti_battle"));
	if (SymbioteMeshFinder.Succeeded())
	{
		SymbioteMesh->SetSkeletalMeshAsset(SymbioteMeshFinder.Object);
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

	// SFX
	struct FSndBind { USoundBase** Ptr; const TCHAR* Path; };
	const FSndBind Sounds[] = {
		{ &PossessSound,   TEXT("/Game/Sounds/sfx_possess.sfx_possess") },
		{ &EjectSound,     TEXT("/Game/Sounds/sfx_eject.sfx_eject") },
		{ &LevelUpSound,   TEXT("/Game/Sounds/sfx_levelup.sfx_levelup") },
		{ &PickSound,      TEXT("/Game/Sounds/sfx_pick.sfx_pick") },
		{ &HurtSound,      TEXT("/Game/Sounds/sfx_hurt.sfx_hurt") },
		{ &GameOverSound,  TEXT("/Game/Sounds/sfx_gameover.sfx_gameover") },
		{ &KnockbackSound, TEXT("/Game/Sounds/sfx_swoosh.sfx_swoosh") },
	};
	for (const FSndBind& B : Sounds)
	{
		ConstructorHelpers::FObjectFinder<USoundBase> F(B.Path);
		if (F.Succeeded())
		{
			*B.Ptr = F.Object;
		}
	}

	// Host form: the possessed goblin (same mesh as enemies, symbiote-tinted).
	HostGoblinMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HostGoblinMesh"));
	HostGoblinMesh->SetupAttachment(RootComponent);
	HostGoblinMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HostGoblinMesh->SetRelativeScale3D(FVector(1.3f)); // a bit bigger than enemy goblins
	HostGoblinMesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	HostGoblinMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	// Layered anim instance: lower body walks while the upper body swings.
	HostGoblinMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	HostGoblinMesh->SetAnimInstanceClass(UGoblinAnimInstance::StaticClass());
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
	Movement->MaxSpeed = 300.f; // just a bit faster than goblins (250)
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

	HostProjectileClass = AVenomProjectile::StaticClass();

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}

void AParasitePawn::BeginPlay()
{
	Super::BeginPlay();

	// FloatingPawnMovement has no gravity, so whatever height we spawn at
	// persists forever. Snap to a fixed hover height above the floor (the
	// meshes are offset -40/-90, tuned for hovering ~90 above the ground).
	{
		const FVector Loc = GetActorLocation();
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		if (GetWorld()->LineTraceSingleByChannel(Hit,
			Loc + FVector(0.f, 0.f, 2000.f), Loc - FVector(0.f, 0.f, 5000.f),
			ECC_Visibility, Params))
		{
			SetActorLocation(FVector(Loc.X, Loc.Y, Hit.Location.Z + 90.f));
		}
	}

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

	// Tint the symbiote creature black-glossy with the pulsing red rim.
	SymbioteMIDs.Reset();
	if (SymbioteMaterial && SymbioteMesh)
	{
		const int32 NumMats = SymbioteMesh->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(SymbioteMaterial, this);
			SymbioteMesh->SetMaterial(i, MID);
			SymbioteMIDs.Add(MID);
		}
	}

	// Same black-glossy goo look for the cluster lumps.
	GooBlobMIDs.Reset();
	UMaterialInterface* GooBase = SymbioteMaterial ? SymbioteMaterial : TintMaterial;
	for (UStaticMeshComponent* Blob : GooBlobs)
	{
		if (Blob && GooBase)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(GooBase, this);
			Blob->SetMaterial(0, MID);
			if (!SymbioteMaterial)
			{
				MID->SetVectorParameterValue(TEXT("Color"), ParasiteColor);
			}
			GooBlobMIDs.Add(MID);
		}
		else
		{
			GooBlobMIDs.Add(nullptr);
		}
	}
}

void AParasitePawn::SetGooBlobsVisible(bool bVisible)
{
	for (UStaticMeshComponent* Blob : GooBlobs)
	{
		if (Blob)
		{
			Blob->SetVisibility(bVisible);
		}
	}
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

void AParasitePawn::SetParasiteVisible(bool bVisible)
{
	if (SymbioteMesh)
	{
		SymbioteMesh->SetVisibility(bVisible);
	}
}

void AParasitePawn::EnterHostForm()
{
	SetParasiteVisible(false);
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
	bHostAttacking = false;
	if (UGoblinAnimInstance* AI = Cast<UGoblinAnimInstance>(HostGoblinMesh->GetAnimInstance()))
	{
		AI->LocomotionAnim = HostIdleAnim;
		AI->AttackAnim = HostAttackAnim;
		AI->bAttacking = false;
		AI->AttackTime = 0.f;
	}
}

void AParasitePawn::ExitHostForm()
{
	HostGoblinMesh->SetVisibility(false);
	HostMIDs.Reset();
	SetParasiteVisible(true);
	SetActorRotation(FRotator::ZeroRotator);
	ApplyBodyColor();
}

void AParasitePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Procedural camera shake.
	if (Camera)
	{
		if (ShakeTimeLeft > 0.f)
		{
			ShakeTimeLeft -= DeltaSeconds;
			const float Alpha = (ShakeDuration > 0.f) ? FMath::Clamp(ShakeTimeLeft / ShakeDuration, 0.f, 1.f) : 0.f;
			const float A = ShakeAmplitude * Alpha;
			Camera->SetRelativeLocation(FVector(FMath::FRandRange(-A, A), FMath::FRandRange(-A, A), FMath::FRandRange(-A, A)));
		}
		else if (!Camera->GetRelativeLocation().IsNearlyZero())
		{
			Camera->SetRelativeLocation(FVector::ZeroVector);
		}
	}

	// Host regeneration (augment): the symbiote slowly mends the worn host body.
	if (bIsPossessing && !bDead && HostRegenRate > 0.f && Health < MaxHealth)
	{
		Health = FMath::Min(MaxHealth, Health + HostRegenRate * DeltaSeconds);
	}

	// Cinematic possession leap: arc onto the host, then latch on landing.
	if (bLeaping)
	{
		LeapElapsed += DeltaSeconds;
		const float A = (LeapDuration > 0.f) ? FMath::Clamp(LeapElapsed / LeapDuration, 0.f, 1.f) : 1.f;

		if (!LeapTarget)
		{
			// Host vanished before we reached it — abort the pounce.
			bLeaping = false;
			bInvulnerable = false;
			EndLiquidForm();
		}
		else
		{
			const FVector T = LeapTarget->GetActorLocation();

			// Always face the host during the whole pounce.
			FVector Dir = T - LeapStart;
			Dir.Z = 0.f;
			if (!Dir.IsNearlyZero())
			{
				SetActorRotation(Dir.Rotation());
			}

			// The whole pounce is a blob of living goo (BodyMesh sphere), not the
			// humanoid symbiote — so it actually reads as liquid.
			if (BodyMID)
			{
				BodyMID->SetScalarParameterValue(TEXT("Pulse"), 1.f); // rim glows hot
			}

			if (LeapElapsed < LeapWindup)
			{
				// --- Phase 0: LIQUEFY in place. The goo melts into a wide puddle,
				// then surges up into a tall wobbling liquid column. ---
				SetActorLocation(FVector(LeapStart.X, LeapStart.Y, LeapStart.Z));

				if (BodyMesh)
				{
					const float W = FMath::Clamp(LeapElapsed / FMath::Max(LeapWindup, 0.0001f), 0.f, 1.f);
					const float Wob = 0.12f * FMath::Sin(W * 40.f); // liquid ripple

					float SZ, SXY;
					if (W < 0.45f)
					{
						const float K = W / 0.45f;                 // melt down into a puddle
						SZ = FMath::Lerp(0.8f, 0.32f, K);
						SXY = FMath::Lerp(0.8f, 1.35f, K);
					}
					else
					{
						const float K = (W - 0.45f) / 0.55f;       // surge up into a column
						SZ = FMath::Lerp(0.32f, 1.9f, K);
						SXY = FMath::Lerp(1.35f, 0.5f, K);
					}
					BodyMesh->SetRelativeScale3D(FVector(SXY - Wob, SXY + Wob, SZ + Wob));
					// Keep the blob's base near the ground as it grows.
					BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, 50.f * SZ - 40.f));
				}

				// Metaball cluster: lumps blend from a wide flat puddle to a rising,
				// stacked column — a living, wobbling mass of goo.
				{
					const float Wc = FMath::Clamp(LeapElapsed / FMath::Max(LeapWindup, 0.0001f), 0.f, 1.f);
					const float ColBlend = FMath::Clamp((Wc - 0.35f) / 0.65f, 0.f, 1.f); // 0 puddle -> 1 column
					const float Time = GetWorld()->GetTimeSeconds();
					for (int32 i = 0; i < GooBlobs.Num(); ++i)
					{
						UStaticMeshComponent* Blob = GooBlobs[i];
						if (!Blob) { continue; }
						const float Ang = i * 2.39996323f; // golden angle spread
						const float Rf = 0.45f + 0.55f * (((i * 37) % 100) / 100.f);
						const float R = FMath::Lerp(62.f * Rf, 16.f * Rf, ColBlend);
						const float X = FMath::Cos(Ang) * R + 4.f * FMath::Sin(Time * 22.f + i);
						const float Y = FMath::Sin(Ang) * R + 4.f * FMath::Cos(Time * 20.f + i);
						const float PudZ = -18.f + 8.f * FMath::Sin(Time * 18.f + i);
						const float ColZ = FMath::Lerp(-25.f, 135.f, (i + 0.5f) / GooBlobs.Num());
						const float Z = FMath::Lerp(PudZ, ColZ, ColBlend);
						Blob->SetRelativeLocation(FVector(X, Y, Z));
						Blob->SetRelativeScale3D(FVector(0.5f - 0.18f * ColBlend + 0.06f * FMath::Sin(Time * 26.f + i)));
						if (GooBlobMIDs.IsValidIndex(i) && GooBlobMIDs[i])
						{
							GooBlobMIDs[i]->SetScalarParameterValue(TEXT("Pulse"), 1.f);
						}
					}
				}
			}
			else
			{
				// --- Phase 1: FLIGHT. The goo droplet arcs onto the host,
				// stretched forward along its travel like a thrown blob. ---
				const float FA = (LeapDuration > 0.f)
					? FMath::Clamp((LeapElapsed - LeapWindup) / LeapDuration, 0.f, 1.f) : 1.f;

				FVector Pos = FMath::Lerp(FVector(LeapStart.X, LeapStart.Y, LeapStart.Z),
										  FVector(T.X, T.Y, LeapStart.Z), FA);
				Pos.Z += LeapArcHeight * FMath::Sin(FA * PI);
				SetActorLocation(Pos);

				if (BodyMesh)
				{
					const float Lunge = FMath::Sin(FA * PI);       // 0 -> 1 -> 0
					// Elongate along the direction of travel (local X after facing).
					const float SX = 0.8f * (1.f + 1.0f * Lunge);
					const float SY = 0.8f * (1.f - 0.30f * Lunge);
					const float SZ = 0.8f * (1.f - 0.30f * Lunge);
					BodyMesh->SetRelativeScale3D(FVector(SX, SY, SZ));
					BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
				}

				// Goo trail: lumps drag behind along the travel axis, shrinking into
				// a comet-like tail of dripping symbiote.
				{
					const float Time = GetWorld()->GetTimeSeconds();
					const float Lunge = FMath::Sin(FA * PI);
					for (int32 i = 0; i < GooBlobs.Num(); ++i)
					{
						UStaticMeshComponent* Blob = GooBlobs[i];
						if (!Blob) { continue; }
						const float K = (i + 1.f) / GooBlobs.Num(); // 0..1 back along the tail
						const float BackX = -95.f * K * (0.4f + 0.9f * Lunge);
						const float WobY = 10.f * FMath::Sin(Time * 30.f + i * 1.7f);
						const float WobZ = 8.f * FMath::Sin(Time * 24.f + i * 2.3f) - 6.f * K;
						Blob->SetRelativeLocation(FVector(BackX, WobY, WobZ));
						Blob->SetRelativeScale3D(FVector(0.5f * (1.f - 0.6f * K)));
						if (GooBlobMIDs.IsValidIndex(i) && GooBlobMIDs[i])
						{
							GooBlobMIDs[i]->SetScalarParameterValue(TEXT("Pulse"), 1.f);
						}
					}
				}

				if (FA >= 1.f)
				{
					AMobEnemy* Landed = LeapTarget;
					bLeaping = false;
					LandOnHost(Landed);
				}
			}
		}
		return; // freeze normal parasite/host logic during the pounce
	}

	// Remember where we're heading — that's where attacks fire.
	{
		FVector Vel = GetVelocity();
		Vel.Z = 0.f;
		if (Vel.SizeSquared() > 400.f)
		{
			AimDirection = Vel.GetSafeNormal();
		}
	}

	// Possession-range ring: only while briefly flagged (a failed possess attempt).
	if (GetWorld()->GetTimeSeconds() < ShowRangeUntil)
	{
		DrawDebugCircle(GetWorld(), GetActorLocation(), PossessRange, 48, FColor::Red,
			/*bPersistent=*/false, /*LifeTime=*/-1.f, /*DepthPriority=*/0, /*Thickness=*/4.f,
			/*YAxis=*/FVector(1.f, 0.f, 0.f), /*ZAxis=*/FVector(0.f, 1.f, 0.f), /*bDrawAxis=*/false);
	}

	// Parasite: symbiote faces its movement, pulses its rim, and — since it has
	// no animations — bobs and squash/stretches procedurally so it looks alive.
	if (!bIsPossessing && SymbioteMesh)
	{
		const float T = GetWorld()->GetTimeSeconds();
		for (UMaterialInstanceDynamic* MID : SymbioteMIDs)
		{
			if (MID)
			{
				MID->SetScalarParameterValue(TEXT("Pulse"), 0.55f + 0.45f * FMath::Sin(T * 4.f));
			}
		}

		FVector Vel = GetVelocity();
		Vel.Z = 0.f;
		const float Speed = Vel.Size();
		if (Speed > 20.f)
		{
			SetActorRotation(Vel.Rotation());
		}
		const float Move01 = FMath::Clamp(Speed / 300.f, 0.f, 1.f);

		// Subtle surge while moving; gentle breathing while idle.
		const float S = FMath::Sin(T * (5.f + 7.f * Move01));
		const float Amp = 0.05f + 0.07f * Move01;
		const float ScaleXY = 60.f * (1.f - Amp * 0.6f * S);
		const float ScaleZ = 60.f * (1.f + Amp * S);
		SymbioteMesh->SetRelativeScale3D(FVector(ScaleXY, ScaleXY, ScaleZ));

		const float Bob = (1.f + 3.f * Move01) * (0.5f + 0.5f * S);
		SymbioteMesh->SetRelativeLocation(FVector(0.f, 0.f, -40.f + Bob));
	}
	else
	{
		// Fresh possession: the symbiote floods over the host — pop it to full size.
		if (bEnveloping && HostGoblinMesh)
		{
			EnvelopElapsed += DeltaSeconds;
			const float E = FMath::Clamp(EnvelopElapsed / 0.18f, 0.f, 1.f);
			HostGoblinMesh->SetRelativeScale3D(FVector(FMath::Lerp(0.4f, 1.3f, E)));
			if (E >= 1.f)
			{
				bEnveloping = false;
				HostGoblinMesh->SetRelativeScale3D(FVector(1.3f));
			}
		}

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
		bHostWalking = bWantWalk;
		if (bHostAttacking)
		{
			// Face the target while swinging — legs keep walking (backpedal-and-slash).
			SetActorRotation(AttackFacing.Rotation());
		}
		else if (bWantWalk)
		{
			SetActorRotation(Vel.Rotation());
		}

		// Drive the layered anim instance: lower body walks, upper body swings.
		if (UGoblinAnimInstance* AI = Cast<UGoblinAnimInstance>(HostGoblinMesh->GetAnimInstance()))
		{
			AI->LocomotionAnim = bWantWalk ? HostWalkAnim : HostIdleAnim;
			AI->bAttacking = bHostAttacking;
			AI->AttackTime = bHostAttacking ? (T - HostAttackStartTime) : 0.f;
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

	// (Augment picker cards are drawn by AVenomHUD.)
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
	if (bLeaping) { return; }
	AddMovementInput(FVector::ForwardVector, Value.Get<float>());
}

void AParasitePawn::MoveBackward(const FInputActionValue& Value)
{
	if (bLeaping) { return; }
	AddMovementInput(FVector::ForwardVector, -Value.Get<float>());
}

void AParasitePawn::MoveLeft(const FInputActionValue& Value)
{
	if (bLeaping) { return; }
	AddMovementInput(FVector::RightVector, -Value.Get<float>());
}

void AParasitePawn::MoveRight(const FInputActionValue& Value)
{
	if (bLeaping) { return; }
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
	if (bChoosingAugment || bLeaping)
	{
		return;
	}

	// Pressing possess with nothing in reach flashes the range ring as feedback.
	{
		bool bAnyInRange = false;
		const FVector MyLoc = GetActorLocation();
		for (TActorIterator<AMobEnemy> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), MyLoc) <= PossessRange)
			{
				bAnyInRange = true;
				break;
			}
		}
		if (!bAnyInRange)
		{
			ShowRangeUntil = GetWorld()->GetTimeSeconds() + 1.2f;
		}
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
		// Flash the possess-range ring so you can see nothing's in reach.
		ShowRangeUntil = GetWorld()->GetTimeSeconds() + 1.2f;
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1, 1.5f, FColor::Orange, TEXT("No host in range"));
		}
		return;
	}

	// Don't teleport — pounce. The symbiote leaps onto the host, then latches.
	StartLeap(Host);
}

void AParasitePawn::StartLeap(AMobEnemy* Host)
{
	bLeaping = true;
	LeapTarget = Host;
	LeapStart = GetActorLocation();
	LeapElapsed = 0.f;
	bInvulnerable = true; // untouchable mid-air during the pounce

	if (Movement)
	{
		Movement->Velocity = FVector::ZeroVector;
	}
	SetSelectedTarget(nullptr);

	// Become liquid: swap the humanoid mesh for the goo blob for the whole pounce.
	EnterLiquidForm();

	// A guttural symbiote lunge — the scary possess sound carries through the arc.
	UGameplayStatics::PlaySound2D(this, PossessSound, 0.9f);
}

void AParasitePawn::EnterLiquidForm()
{
	SetParasiteVisible(false); // hide the humanoid symbiote
	if (BodyMesh)
	{
		BodyMesh->SetVisibility(true);
	}
	SetGooBlobsVisible(true);
}

void AParasitePawn::EndLiquidForm()
{
	if (BodyMesh)
	{
		BodyMesh->SetVisibility(false);
		BodyMesh->SetRelativeScale3D(FVector(0.8f));
		BodyMesh->SetRelativeLocation(FVector::ZeroVector);
	}
	SetGooBlobsVisible(false);
	if (SymbioteMesh)
	{
		SymbioteMesh->SetRelativeScale3D(FVector(60.f));
		SymbioteMesh->SetRelativeLocation(FVector(0.f, 0.f, -40.f));
	}
	SetParasiteVisible(true);
}

void AParasitePawn::LandOnHost(AMobEnemy* Host)
{
	if (!Host)
	{
		// Target died mid-air — drop back to a bare parasite.
		bInvulnerable = false;
		EndLiquidForm();
		return;
	}

	// Snap onto the host's spot and wear its body.
	const FVector HostLoc = Host->GetActorLocation();
	SetActorLocation(FVector(HostLoc.X, HostLoc.Y, LeapStart.Z));

	bIsPossessing = true;
	bInvulnerable = false;
	bHostRanged = Host->IsRanged();

	// Put the goo blob away; the host body takes over.
	if (BodyMesh)
	{
		BodyMesh->SetVisibility(false);
		BodyMesh->SetRelativeScale3D(FVector(0.8f));
		BodyMesh->SetRelativeLocation(FVector::ZeroVector);
	}
	SetGooBlobsVisible(false);
	EnterHostForm();

	// Envelop pop: the symbiote floods over the host, swelling it to full size.
	if (HostGoblinMesh)
	{
		HostGoblinMesh->SetRelativeScale3D(FVector(0.4f));
	}
	bEnveloping = true;
	EnvelopElapsed = 0.f;

	// Inherit the host's current HP (possess a healthy one!).
	MaxHealth = Host->GetMaxHealth();
	Health = Host->GetHealth();

	// Start the possess cooldown.
	bPossessReady = false;
	GetWorldTimerManager().SetTimer(PossessCDTimer, this, &AParasitePawn::OnPossessReady, PossessCooldown, false);

	Host->Destroy();

	// Impact: a heavier shake on the latch.
	AddCameraShake(11.f, 0.3f);

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
	if (bLeaping) { return; } // no attacking mid-pounce

	// Stats depend on the current form.
	const bool bHost = bIsPossessing;
	const float Range = bHost ? (bHostRanged ? HostRangedAttackRange : HostAttackRange) : ParasiteAttackRange;
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

	// Ranged host: throw projectiles at the target instead of a melee wedge.
	if (bHost && bHostRanged)
	{
		if (HostProjectileClass && GetWorld())
		{
			const int32 Shots = FMath::Max(1, ProjectileCount);
			const float SpreadStep = 12.f; // degrees between adjacent shots
			const float StartAngle = -SpreadStep * (Shots - 1) * 0.5f;
			for (int32 s = 0; s < Shots; ++s)
			{
				const float Angle = StartAngle + SpreadStep * s;
				const FVector Dir = Aim.RotateAngleAxis(Angle, FVector::UpVector);
				const FVector Muzzle = MyLoc + FVector(0.f, 0.f, 15.f) + Dir * 10.f;
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AVenomProjectile* Proj = GetWorld()->SpawnActor<AVenomProjectile>(HostProjectileClass, Muzzle, Dir.Rotation(), Params))
				{
					Proj->Launch(Dir, HostProjectileSpeed, HostDamage, /*bHitMobs=*/true, PiercePower, BounceCharges);
				}
			}
		}
		if (KnockbackSound)
		{
			UGameplayStatics::PlaySound2D(this, KnockbackSound, 0.4f);
		}
		if (HostAttackAnim && HostGoblinMesh && !bHostAttacking)
		{
			bHostAttacking = true;
			HostAttackStartTime = GetWorld()->GetTimeSeconds();
			GetWorldTimerManager().SetTimer(HostAttackTimer, this,
				&AParasitePawn::OnHostAttackDone, HostAttackAnim->GetPlayLength(), false);
		}
		return;
	}

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

	if (bHost)
	{
		// Host melee is single-target: strike only the nearest mob in the wedge.
		AMobEnemy* MeleeTarget = nullptr;
		float BestMeleeDistSq = TNumericLimits<float>::Max();
		for (AMobEnemy* Mob : Targets)
		{
			const float DistSq = FVector::DistSquared2D(Mob->GetActorLocation(), MyLoc);
			if (DistSq < BestMeleeDistSq)
			{
				BestMeleeDistSq = DistSq;
				MeleeTarget = Mob;
			}
		}
		if (MeleeTarget && Damage > 0.f)
		{
			MeleeTarget->TakeHit(Damage);
		}
	}
	else
	{
		// Parasite shove: knock the whole wedge back (crowd control, no damage).
		for (AMobEnemy* Mob : Targets)
		{
			if (Knockback > 0.f)
			{
				FVector Away = Mob->GetActorLocation() - MyLoc;
				Away.Z = 0.f;
				Mob->ApplyKnockback(Away.GetSafeNormal() * Knockback);
			}
		}
	}

	// Parasite shove: swoosh when it knocks mobs back.
	if (!bHost && Targets.Num() > 0)
	{
		UGameplayStatics::PlaySound2D(this, KnockbackSound, 0.5f);
	}

	// Host swings its weapon when it actually connects.
	if (bHost && Targets.Num() > 0 && HostAttackAnim && !bHostAttacking)
	{
		bHostAttacking = true;
		HostAttackStartTime = GetWorld()->GetTimeSeconds();
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
	// The swing is over; the layered anim instance eases the upper body back to
	// the locomotion pose on its own (Tick keeps LocomotionAnim current).
	bHostAttacking = false;
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

	PlayUISound(LevelUpSound);

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

	// Offer 3 distinct augments from the pool (ids 0..10).
	TArray<int32> Pool = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
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

	// Show the cursor so the HUD cards can be clicked.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
	}
}

void AParasitePawn::ChooseAugment(int32 OptionIndex)
{
	if (!bChoosingAugment || !CurrentAugmentOptions.IsValidIndex(OptionIndex))
	{
		return;
	}

	ApplyAugment(CurrentAugmentOptions[OptionIndex]);
	PlayUISound(PickSound);

	bChoosingAugment = false;
	--PendingLevelUps;

	if (PendingLevelUps > 0)
	{
		StartAugmentChoice(); // queue the next choice, stay paused
	}
	else
	{
		UGameplayStatics::SetGamePaused(this, false);

		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->bShowMouseCursor = false;
			PC->bEnableClickEvents = false;
			PC->bEnableMouseOverEvents = false;
		}
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
	case 5: // projectile pierce
		PiercePower += 1;
		break;
	case 6: // extra projectile per shot
		ProjectileCount += 1;
		break;
	case 7: // host regeneration (숙주 재생)
		HostRegenRate += 4.f;
		break;
	case 8: // faster infection: shorter possess cooldown (감염 속도↑)
		PossessCooldown = FMath::Max(2.f, PossessCooldown - 2.f);
		break;
	case 9: // tougher parasite: more bare-parasite HP for the eject window
		ParasiteMaxHP += 15.f;
		if (!bIsPossessing)
		{
			MaxHealth = ParasiteMaxHP;
			Health += 15.f;
		}
		break;
	case 10: // projectiles ricochet off cover
		BounceCharges += 1;
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
	case 5: return TEXT("Projectiles pierce +1 enemy");
	case 6: return TEXT("+1 projectile per shot");
	case 7: return TEXT("Host regenerates +4 HP/sec");
	case 8: return TEXT("-2s possess cooldown");
	case 9: return TEXT("+15 Parasite Max HP");
	case 10: return TEXT("Projectiles ricochet off cover");
	default: return TEXT("Unknown");
	}
}

FString AParasitePawn::AugmentTitle(int32 AugmentId) const
{
	switch (AugmentId)
	{
	case 0: return TEXT("BRUTE FORCE");
	case 1: return TEXT("FRENZY");
	case 2: return TEXT("SWIFT OOZE");
	case 3: return TEXT("LONG REACH");
	case 4: return TEXT("REPULSION");
	case 5: return TEXT("PIERCING SHOT");
	case 6: return TEXT("MULTISHOT");
	case 7: return TEXT("REGENERATION");
	case 8: return TEXT("VIRULENCE");
	case 9: return TEXT("CARAPACE");
	case 10: return TEXT("RICOCHET");
	default: return TEXT("UNKNOWN");
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

	UGameplayStatics::PlaySound2D(this, HurtSound, 0.5f);
	AddCameraShake(5.f, 0.15f);

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
	bHostRanged = false;
	BodyMesh->SetRelativeScale3D(FVector(0.8f));
	ExitHostForm();

	MaxHealth = ParasiteMaxHP;
	Health = ParasiteMaxHP;

	UGameplayStatics::PlaySound2D(this, EjectSound, 0.9f);
	AddCameraShake(11.f, 0.3f);

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

void AParasitePawn::PlayUISound(USoundBase* Sound)
{
	if (!Sound)
	{
		return;
	}
	if (UAudioComponent* AC = UGameplayStatics::SpawnSound2D(this, Sound))
	{
		AC->bIsUISound = true; // keeps playing while the game is paused
	}
}

void AParasitePawn::AddCameraShake(float Amplitude, float Duration)
{
	ShakeAmplitude = FMath::Max(ShakeAmplitude, Amplitude);
	ShakeDuration = FMath::Max(ShakeDuration, Duration);
	ShakeTimeLeft = FMath::Max(ShakeTimeLeft, Duration);
}

void AParasitePawn::GameOver()
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	Health = 0.f;
	PlayUISound(GameOverSound);
	AddCameraShake(14.f, 0.5f);
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
