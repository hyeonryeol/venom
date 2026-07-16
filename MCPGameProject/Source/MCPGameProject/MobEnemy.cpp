// venom — Enemy goblin mob. Chases the player; a possession host.

#include "MobEnemy.h"
#include "ParasitePawn.h"
#include "VenomProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Sound/SoundBase.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AMobEnemy::AMobEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(45.f);
	// Overlap the player (for possession) but don't block movement — EXCEPT
	// solid cover: as a Pawn that blocks WorldStatic, obstacles stop us too.
	CollisionComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CollisionComp->SetCollisionObjectType(ECC_Pawn);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	RootComponent = CollisionComp;

	BodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeScale3D(FVector(MeshScale));
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, MeshZOffset));
	BodyMesh->SetRelativeRotation(FRotator(0.f, MeshYaw, 0.f));
	BodyMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshFinder(TEXT("/Game/Goblin_low_for_bake.Goblin_low_for_bake"));
	if (MeshFinder.Succeeded())
	{
		BodyMesh->SetSkeletalMeshAsset(MeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkFinder(TEXT("/Game/Goblin_low_for_bake_Anim_Anim_Full_Walk.Goblin_low_for_bake_Anim_Anim_Full_Walk"));
	if (WalkFinder.Succeeded())
	{
		WalkAnim = WalkFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UAnimSequence> AttackFinder(TEXT("/Game/Goblin_low_for_bake_Anim_Anim_Full_Attack_One_Handed.Goblin_low_for_bake_Anim_Anim_Full_Attack_One_Handed"));
	if (AttackFinder.Succeeded())
	{
		AttackAnim = AttackFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UAnimSequence> DeathFinder(TEXT("/Game/Goblin_low_for_bake_Anim_Anim_Full_Death.Goblin_low_for_bake_Anim_Anim_Full_Death"));
	if (DeathFinder.Succeeded())
	{
		DeathAnim = DeathFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunFinder(TEXT("/Game/Goblin_low_for_bake_Anim_Anim_Full_Run.Goblin_low_for_bake_Anim_Anim_Full_Run"));
	if (RunFinder.Succeeded())
	{
		RunAnim = RunFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UAnimSequence> JumpFinder(TEXT("/Game/Goblin_low_for_bake_Anim_Anim_Full_Jump.Goblin_low_for_bake_Anim_Anim_Full_Jump"));
	if (JumpFinder.Succeeded())
	{
		JumpAnim = JumpFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> HitSndFinder(TEXT("/Game/Sounds/sfx_hit.sfx_hit"));
	if (HitSndFinder.Succeeded())
	{
		HitSound = HitSndFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<USoundBase> DeathSndFinder(TEXT("/Game/Sounds/sfx_mobdeath.sfx_mobdeath"));
	if (DeathSndFinder.Succeeded())
	{
		DeathSound = DeathSndFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlashFinder(TEXT("/Game/Materials/M_HitFlash.M_HitFlash"));
	if (FlashFinder.Succeeded())
	{
		HitFlashOverlay = FlashFinder.Object;
	}

	ProjectileClass = AVenomProjectile::StaticClass();
}

void AMobEnemy::Shoot(const FVector& TargetLoc)
{
	if (!ProjectileClass || !GetWorld())
	{
		return;
	}
	const FVector Muzzle = GetActorLocation() + FVector(0.f, 0.f, 15.f);
	FVector Dir = TargetLoc - Muzzle;
	Dir.Z = 0.f;
	Dir = Dir.GetSafeNormal();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVenomProjectile* Proj = GetWorld()->SpawnActor<AVenomProjectile>(
		ProjectileClass, Muzzle + Dir * 10.f, Dir.Rotation(), Params);
	if (Proj)
	{
		Proj->Launch(Dir, ProjectileSpeed, ProjectileDamage, /*bHitMobs=*/false);
	}

	// Reuse the melee swing anim as the "cast/shoot" gesture.
	if (AttackAnim && BodyMesh && !bAttacking)
	{
		bAttacking = true;
		BodyMesh->PlayAnimation(AttackAnim, false);
		GetWorldTimerManager().SetTimer(AttackAnimTimer, this,
			&AMobEnemy::OnAttackAnimDone, AttackAnim->GetPlayLength(), false);
	}
}

void AMobEnemy::BeginPlay()
{
	Super::BeginPlay();

	// Snap to a fixed hover height above the floor. Spawns inherit the
	// player's Z, which can be mid-leap (airborne) — and we keep our Z
	// forever, so a bad start would leave this mob floating.
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

	Health = MaxHealth;

	// The movement variant is assigned by the wave director before spawn finished.
	if (bRunner && RunAnim)
	{
		MoveSpeed *= RunSpeedMultiplier;
	}
	else
	{
		bRunner = false; // no run anim -> fall back to a walker
	}

	PlayLoop(LocoAnim());

	if (BaseOverlay && BodyMesh)
	{
		BodyMesh->SetOverlayMaterial(BaseOverlay);
	}
}

UAnimSequence* AMobEnemy::LocoAnim() const
{
	return (bRunner && RunAnim) ? RunAnim : WalkAnim;
}

void AMobEnemy::PlayLoop(UAnimSequence* Anim)
{
	if (Anim && BodyMesh)
	{
		BodyMesh->PlayAnimation(Anim, true);
	}
}

void AMobEnemy::SetHighlighted(bool bInHighlighted)
{
	if (BodyMesh)
	{
		BodyMesh->SetRelativeScale3D(FVector(MeshScale * (bInHighlighted ? 1.25f : 1.0f)));
	}
}

void AMobEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDying)
	{
		return;
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}

	const FVector MyLoc = GetActorLocation();
	const float Now = GetWorld()->GetTimeSeconds();

	// Jumper variant: periodically leap at the player in an arc.
	if (bMidJump)
	{
		JumpElapsed += DeltaSeconds;
		const float A = (JumpDuration > 0.f) ? FMath::Clamp(JumpElapsed / JumpDuration, 0.f, 1.f) : 1.f;
		FVector P = FMath::Lerp(JumpStart, JumpEnd, A);
		P.Z = JumpStart.Z + JumpArcHeight * FMath::Sin(A * PI);
		SetActorLocation(P, /*bSweep=*/false);

		FVector Face = JumpEnd - JumpStart;
		Face.Z = 0.f;
		if (!Face.IsNearlyZero())
		{
			SetActorRotation(Face.Rotation());
		}
		if (A >= 1.f)
		{
			bMidJump = false;
			SetActorLocation(FVector(JumpEnd.X, JumpEnd.Y, JumpStart.Z), false);
			if (!bDying)
			{
				PlayLoop(LocoAnim());
			}
		}
		return; // no chase/attack while airborne
	}

	if (bJumper && !bAttacking && JumpAnim)
	{
		const float DistToPlayer = FVector::Dist2D(Player->GetActorLocation(), MyLoc);
		if (Now - LastJumpTime >= JumpInterval && DistToPlayer > 160.f && DistToPlayer < 1000.f)
		{
			LastJumpTime = Now;
			bMidJump = true;
			JumpElapsed = 0.f;
			JumpStart = MyLoc;
			FVector Dir = Player->GetActorLocation() - MyLoc;
			Dir.Z = 0.f;
			Dir = Dir.GetSafeNormal();
			// Leap toward the player, but stop short of their body.
			const float LeapDist = FMath::Clamp(DistToPlayer - MinPlayerDistance, 150.f, JumpMaxDistance);
			JumpEnd = MyLoc + Dir * LeapDist;
			BodyMesh->PlayAnimation(JumpAnim, false);
			return;
		}
	}

	// Chase: head straight for the player (on their plane).
	FVector ToPlayer = Player->GetActorLocation() - MyLoc;
	ToPlayer.Z = 0.f;
	const FVector ChaseDir = ToPlayer.GetSafeNormal();

	// Separation: push away from nearby mobs so they spread instead of stacking.
	FVector Separation = FVector::ZeroVector;
	const float SepRadiusSq = SeparationRadius * SeparationRadius;
	for (TActorIterator<AMobEnemy> It(GetWorld()); It; ++It)
	{
		AMobEnemy* Other = *It;
		if (Other == this)
		{
			continue;
		}
		FVector Away = MyLoc - Other->GetActorLocation();
		Away.Z = 0.f;
		const float DistSq = Away.SizeSquared();
		if (DistSq > KINDA_SMALL_NUMBER && DistSq < SepRadiusSq)
		{
			const float Dist = FMath::Sqrt(DistSq);
			Separation += (Away / Dist) * (1.f - Dist / SeparationRadius);
		}
	}

	FVector Desired = ChaseDir + Separation * SeparationWeight;
	Desired.Z = 0.f;

	FVector Delta = FVector::ZeroVector;
	if (Desired.SizeSquared() > KINDA_SMALL_NUMBER)
	{
		Delta += Desired.GetSafeNormal() * MoveSpeed * DeltaSeconds;
	}
	Delta += KnockbackVelocity * DeltaSeconds;

	FVector NewLoc = MyLoc + Delta;

	// Never overlap the player's body — hold at MinPlayerDistance.
	FVector FromPlayer = NewLoc - Player->GetActorLocation();
	FromPlayer.Z = 0.f;
	const float DistToPlayer = FromPlayer.Size();
	if (DistToPlayer > KINDA_SMALL_NUMBER && DistToPlayer < MinPlayerDistance)
	{
		NewLoc = Player->GetActorLocation() + (FromPlayer / DistToPlayer) * MinPlayerDistance;
		NewLoc.Z = MyLoc.Z;
	}

	if (!NewLoc.Equals(MyLoc))
	{
		// Sweep so solid cover (pillars/boulders) stops us instead of clipping.
		SetActorLocation(NewLoc, /*bSweep=*/true);
	}
	KnockbackVelocity = FMath::VInterpTo(KnockbackVelocity, FVector::ZeroVector, DeltaSeconds, KnockbackDecay);

	if (!ChaseDir.IsNearlyZero())
	{
		SetActorRotation(ChaseDir.Rotation());
	}

	// Ranged: fire a projectile at the player from a distance.
	if (bRanged)
	{
		if (FVector::Dist2D(Player->GetActorLocation(), MyLoc) <= ShootRange &&
			Now - LastShootTime >= ShootCooldown)
		{
			LastShootTime = Now;
			Shoot(Player->GetActorLocation());
		}
	}

	// Contact attack: bite the player on a cooldown while touching.
	if (AParasitePawn* Parasite = Cast<AParasitePawn>(Player))
	{
		if (FVector::Dist2D(Player->GetActorLocation(), MyLoc) <= ContactRange)
		{
			if (Now - LastAttackTime >= AttackCooldown)
			{
				LastAttackTime = Now;
				Parasite->ReceiveContactDamage(ContactDamage);

				// Swing animation (once), then back to walking.
				if (AttackAnim && BodyMesh && !bAttacking)
				{
					bAttacking = true;
					BodyMesh->PlayAnimation(AttackAnim, false);
					GetWorldTimerManager().SetTimer(AttackAnimTimer, this,
						&AMobEnemy::OnAttackAnimDone, AttackAnim->GetPlayLength(), false);
				}
			}
		}
	}
}

void AMobEnemy::OnAttackAnimDone()
{
	bAttacking = false;
	if (!bDying)
	{
		PlayLoop(LocoAnim());
	}
}

void AMobEnemy::TakeHit(float DamageAmount)
{
	if (bDying)
	{
		return;
	}
	Health -= DamageAmount;

	if (Health <= 0.f)
	{
		Die();
		return;
	}

	// Juice: hit sound + brief white overlay flash.
	if (HitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HitSound, GetActorLocation(), 0.6f);
	}
	if (HitFlashOverlay && BodyMesh)
	{
		BodyMesh->SetOverlayMaterial(HitFlashOverlay);
		GetWorldTimerManager().SetTimer(HitFlashTimer, this, &AMobEnemy::ClearHitFlash, 0.14f, false);
	}
}

void AMobEnemy::ClearHitFlash()
{
	if (BodyMesh)
	{
		BodyMesh->SetOverlayMaterial(BaseOverlay); // restore blue tint for ranged, else none
	}
}

void AMobEnemy::Die()
{
	if (bDying)
	{
		return;
	}
	bDying = true;

	ClearHitFlash();
	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation(), 0.7f);
	}

	if (AParasitePawn* Player = Cast<AParasitePawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->AddXP(XPReward);
	}

	float DeathLen = 1.f;
	if (DeathAnim && BodyMesh)
	{
		BodyMesh->PlayAnimation(DeathAnim, false);
		DeathLen = DeathAnim->GetPlayLength();
	}

	// Remove after the death animation plays out.
	FTimerHandle& Handle = DeathTimer;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		Destroy();
	}), DeathLen, false);
}

void AMobEnemy::ApplyKnockback(const FVector& Impulse)
{
	KnockbackVelocity += Impulse;
}
