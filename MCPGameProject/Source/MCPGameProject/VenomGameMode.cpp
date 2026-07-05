// venom — Game mode: makes the parasite the player pawn and spawns mobs.

#include "VenomGameMode.h"
#include "ParasitePawn.h"
#include "MobEnemy.h"

#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AVenomGameMode::AVenomGameMode()
{
	DefaultPawnClass = AParasitePawn::StaticClass();
	MobClass = AMobEnemy::StaticClass();
}

void AVenomGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Start the drip-feed of chasers a second after play begins.
	GetWorldTimerManager().SetTimer(SpawnTimer, this, &AVenomGameMode::SpawnMob, SpawnInterval, true, 1.0f);
}

void AVenomGameMode::SpawnMob()
{
	UWorld* World = GetWorld();
	if (!World || !MobClass)
	{
		return;
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}

	// Respect the alive-mob cap.
	int32 Count = 0;
	for (TActorIterator<AMobEnemy> It(World); It; ++It)
	{
		++Count;
	}
	if (Count >= MaxMobs)
	{
		return;
	}

	// Spawn on a ring around the player so they close in from the edges.
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const FVector Offset(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
	const FVector PlayerLoc = Player->GetActorLocation();
	const FVector SpawnLoc = PlayerLoc + Offset * SpawnRadius;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	World->SpawnActor<AMobEnemy>(MobClass, SpawnLoc, FRotator::ZeroRotator, Params);
}
