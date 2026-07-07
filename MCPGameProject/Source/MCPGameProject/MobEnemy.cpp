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
	// Overlap the player (for possession) but don't block movement.
	CollisionComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
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
	const FVector Muzzle = GetActorLocation() + FVector(0.f, 0.f, 60.f);
	FVector Dir = TargetLoc - Muzzle;
	Dir.Z = 0.f;
	Dir = Dir.GetSafeNormal();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVenomProjectile* Proj = GetWorld()->SpawnActor<AVenomProjectile>(
		ProjectileClass, Muzzle + Dir * 60.f, Dir.Rotation(), Params);
	if (Proj)
	{
		Proj->Launch(Dir, ProjectileSpeed, ProjectileDamage);
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

	Health = MaxHealth;
	PlayLoop(WalkAnim);
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
		SetActorLocation(NewLoc, /*bSweep=*/false);
	}
	KnockbackVelocity = FMath::VInterpTo(KnockbackVelocity, FVector::ZeroVector, DeltaSeconds, KnockbackDecay);

	if (!ChaseDir.IsNearlyZero())
	{
		SetActorRotation(ChaseDir.Rotation());
	}

	// Ranged: fire a projectile at the player from a distance.
	if (bRanged)
	{
		const float Now = GetWorld()->GetTimeSeconds();
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
			const float Now = GetWorld()->GetTimeSeconds();
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
		PlayLoop(WalkAnim);
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
		GetWorldTimerManager().SetTimer(HitFlashTimer, this, &AMobEnemy::ClearHitFlash, 0.07f, false);
	}
}

void AMobEnemy::ClearHitFlash()
{
	if (BodyMesh)
	{
		BodyMesh->SetOverlayMaterial(nullptr);
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
