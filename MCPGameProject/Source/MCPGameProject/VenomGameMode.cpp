// venom — Game mode: makes the parasite the player pawn and runs the wave director.

#include "VenomGameMode.h"
#include "ParasitePawn.h"
#include "MobEnemy.h"
#include "MobRangedGoblin.h"
#include "VenomObstacle.h"
#include "VenomPlayerController.h"
#include "VenomHUD.h"
#include "GameFramework/PlayerStart.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AVenomGameMode::AVenomGameMode()
{
	DefaultPawnClass = AParasitePawn::StaticClass();
	PlayerControllerClass = AVenomPlayerController::StaticClass();
	HUDClass = AVenomHUD::StaticClass();
	MobClass = AMobEnemy::StaticClass();
	RangedMobClass = AMobRangedGoblin::StaticClass();
	ObstacleClass = AVenomObstacle::StaticClass();
}

void AVenomGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Scatter cover before the first mobs arrive.
	SpawnObstacles();

	// Wave 1 begins a beat after play starts; waves advance on WaveTimer.
	GetWorldTimerManager().SetTimer(WaveTimer, this, &AVenomGameMode::NextWave, WaveDuration, true, WaveDuration);
	StartWave(1);
}

float AVenomGameMode::GroundZAt(const FVector& Loc) const
{
	if (const UWorld* World = GetWorld())
	{
		FHitResult Hit;
		const FVector Start = FVector(Loc.X, Loc.Y, 2000.f);
		const FVector End = FVector(Loc.X, Loc.Y, -5000.f);
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility))
		{
			return Hit.Location.Z;
		}
	}
	return 0.f;
}

void AVenomGameMode::SpawnObstacles()
{
	UWorld* World = GetWorld();
	const int32 Total = NumObstacles + NumRocks;
	if (!World || Total <= 0)
	{
		return;
	}

	TSubclassOf<AVenomObstacle> Class = ObstacleClass;
	if (!Class)
	{
		Class = AVenomObstacle::StaticClass();
	}

	// Ring around the actual spawn point — the player may not exist yet at
	// GameMode BeginPlay, and PlayerStart is NOT at the world origin, so
	// falling back to (0,0,0) could drop a boulder right onto the spawn.
	FVector Center = FVector::ZeroVector;
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Center = Player->GetActorLocation();
	}
	else if (const AActor* Start = UGameplayStatics::GetActorOfClass(this, APlayerStart::StaticClass()))
	{
		Center = Start->GetActorLocation();
	}

	// Golden-angle spread keeps cover from clumping; radius jitter varies depth.
	const float GoldenAngle = 2.399963f; // ~137.5 deg
	for (int32 i = 0; i < Total; ++i)
	{
		const float Angle = i * GoldenAngle + FMath::FRandRange(-0.2f, 0.2f);
		const float Radius = FMath::FRandRange(ObstacleMinRadius, ObstacleMaxRadius);
		FVector Loc = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
		Loc.Z = GroundZAt(Loc); // sit on the floor, not at the player's height

		const FTransform Xform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Loc);
		AVenomObstacle* Obstacle = World->SpawnActorDeferred<AVenomObstacle>(
			Class, Xform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Obstacle)
		{
			Obstacle->SetRock(i >= NumObstacles); // first N are pillars, rest boulders
			UGameplayStatics::FinishSpawningActor(Obstacle, Xform);
		}
	}
}

void AVenomGameMode::NextWave()
{
	StartWave(WaveIndex + 1);
}

float AVenomGameMode::GetWaveTimeRemaining() const
{
	if (const UWorld* World = GetWorld())
	{
		return FMath::Max(0.f, World->GetTimerManager().GetTimerRemaining(WaveTimer));
	}
	return 0.f;
}

void AVenomGameMode::StartWave(int32 N)
{
	WaveIndex = FMath::Max(1, N);
	const int32 W = WaveIndex;

	// Defaults: basic melee walkers only.
	bCurRunner = bCurJumper = bCurRanged = false;
	CurRunnerChance = CurJumperChance = CurRangedChance = 0.f;
	bCurHorde = false;

	if (W == 1)
	{
		// Ease-in: plain goblins.
		CurSpawnInterval = 1.7f;
		CurMaxMobs = 12;
	}
	else if (W == 2)
	{
		// Introduce the movers: runners and jumpers.
		bCurRunner = bCurJumper = true;
		CurRunnerChance = 0.28f;
		CurJumperChance = 0.28f;
		CurSpawnInterval = 1.45f;
		CurMaxMobs = 18;
	}
	else if (W == 3)
	{
		// Introduce ranged goblins.
		bCurRunner = bCurJumper = bCurRanged = true;
		CurRunnerChance = 0.22f;
		CurJumperChance = 0.22f;
		CurRangedChance = 0.28f;
		CurSpawnInterval = 1.25f;
		CurMaxMobs = 24;
	}
	else if (W == 4)
	{
		// Hordes: clusters charge in from one side.
		bCurRunner = bCurJumper = bCurRanged = true;
		CurRunnerChance = 0.22f;
		CurJumperChance = 0.22f;
		CurRangedChance = 0.25f;
		CurSpawnInterval = 1.15f;
		CurMaxMobs = 30;
		bCurHorde = true;
		CurHordeInterval = 11.f;
		CurHordeSize = 10;
	}
	else
	{
		// Wave 5+: everything, escalating density and bigger, more frequent hordes.
		const int32 X = W - 4;
		bCurRunner = bCurJumper = bCurRanged = true;
		CurRunnerChance = 0.22f;
		CurJumperChance = 0.22f;
		CurRangedChance = 0.28f;
		CurSpawnInterval = FMath::Max(0.5f, 1.1f - X * 0.06f);
		CurMaxMobs = FMath::Min(60, 30 + X * 4);
		bCurHorde = true;
		CurHordeInterval = FMath::Max(6.f, 10.f - X * 0.5f);
		CurHordeSize = FMath::Min(22, 10 + X * 2);
	}

	// (Re)arm the trickle spawn timer at the wave's cadence.
	GetWorldTimerManager().SetTimer(SpawnTimer, this, &AVenomGameMode::SpawnMob, CurSpawnInterval, true, 0.5f);

	// (Re)arm or clear the horde timer.
	if (bCurHorde)
	{
		GetWorldTimerManager().SetTimer(HordeTimer, this, &AVenomGameMode::SpawnHorde, CurHordeInterval, true, CurHordeInterval);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(HordeTimer);
	}

	// Brief flash on wave change; the HUD shows the persistent wave counter.
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(10, 2.5f, FColor::Yellow,
			FString::Printf(TEXT("=== WAVE %d ==="), WaveIndex));
	}
}

int32 AVenomGameMode::CountAliveMobs() const
{
	int32 Count = 0;
	for (TActorIterator<AMobEnemy> It(GetWorld()); It; ++It)
	{
		++Count;
	}
	return Count;
}

FVector AVenomGameMode::RingSpawnLocation(float AngleRad, float Radius) const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector PlayerLoc = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	const FVector Offset(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.f);
	return PlayerLoc + Offset * Radius;
}

AMobEnemy* AVenomGameMode::SpawnOne(TSubclassOf<AMobEnemy> Class, const FVector& Loc, bool bRunner, bool bJumper)
{
	UWorld* World = GetWorld();
	if (!World || !Class)
	{
		return nullptr;
	}

	const FTransform Xform(FRotator::ZeroRotator, Loc);
	AMobEnemy* Mob = World->SpawnActorDeferred<AMobEnemy>(
		Class, Xform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (Mob)
	{
		Mob->SetVariant(bRunner, bJumper);
		UGameplayStatics::FinishSpawningActor(Mob, Xform);
	}
	return Mob;
}

void AVenomGameMode::SpawnMob()
{
	UWorld* World = GetWorld();
	if (!World || !MobClass || !UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return;
	}
	if (CountAliveMobs() >= CurMaxMobs)
	{
		return;
	}

	// Pick class: ranged or melee.
	TSubclassOf<AMobEnemy> ToSpawn = MobClass;
	if (bCurRanged && RangedMobClass && FMath::FRand() < CurRangedChance)
	{
		ToSpawn = RangedMobClass;
	}

	// Pick a movement variant (runner or jumper, else basic walker).
	bool bRunner = false;
	bool bJumper = false;
	if (bCurRunner || bCurJumper)
	{
		const float R = FMath::FRand();
		if (bCurJumper && R < CurJumperChance)
		{
			bJumper = true;
		}
		else if (bCurRunner && R < CurJumperChance + CurRunnerChance)
		{
			bRunner = true;
		}
	}

	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	SpawnOne(ToSpawn, RingSpawnLocation(Angle, SpawnRadius), bRunner, bJumper);
}

void AVenomGameMode::SpawnHorde()
{
	UWorld* World = GetWorld();
	if (!World || !MobClass || !UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return;
	}

	// A pack rushes in from one direction — mostly plain goblins for a clean swarm.
	const float BaseAngle = FMath::FRandRange(0.f, 2.f * PI);
	const int32 Cap = CurMaxMobs + CurHordeSize; // hordes may briefly exceed the trickle cap
	for (int32 i = 0; i < CurHordeSize; ++i)
	{
		if (CountAliveMobs() >= Cap)
		{
			break;
		}
		const float Angle = BaseAngle + FMath::FRandRange(-0.5f, 0.5f);
		const float Radius = SpawnRadius + FMath::FRandRange(-150.f, 150.f);

		// A few runners salt the pack from wave 4 on; the rest are basic walkers.
		const bool bRunner = bCurRunner && FMath::FRand() < 0.25f;
		SpawnOne(MobClass, RingSpawnLocation(Angle, Radius), bRunner, /*bJumper=*/false);
	}
}
