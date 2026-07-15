// venom — Game mode: makes the parasite the player pawn and runs the wave director.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VenomGameMode.generated.h"

class AMobEnemy;
class AVenomObstacle;

UCLASS()
class MCPGAMEPROJECT_API AVenomGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVenomGameMode();

	int32 GetCurrentWave() const { return WaveIndex; }

	/** Seconds until the next wave begins. */
	float GetWaveTimeRemaining() const;

protected:
	virtual void BeginPlay() override;

	// --- Wave director ---
	/** Begin wave N: reconfigure spawn cadence, allowed types, and hordes. */
	void StartWave(int32 N);
	void NextWave();

	/** Spawns one mob on the ring, up to the wave's alive cap. */
	void SpawnMob();

	/** Spawns a cluster of goblins charging in from one direction. */
	void SpawnHorde();

	/** Scatters cover pillars around the arena once at play start. */
	void SpawnObstacles();

	/** Spawn helper: deferred so the movement variant is set before BeginPlay. */
	AMobEnemy* SpawnOne(TSubclassOf<AMobEnemy> Class, const FVector& Loc, bool bRunner, bool bJumper);

	int32 CountAliveMobs() const;
	FVector RingSpawnLocation(float AngleRad, float Radius) const;

	UPROPERTY(EditAnywhere, Category = "Spawn")
	TSubclassOf<AMobEnemy> MobClass;

	UPROPERTY(EditAnywhere, Category = "Spawn")
	TSubclassOf<AMobEnemy> RangedMobClass;

	UPROPERTY(EditAnywhere, Category = "Spawn")
	float SpawnRadius = 1500.f;

	// --- Obstacles (cover pillars) ---
	UPROPERTY(EditAnywhere, Category = "Obstacles")
	TSubclassOf<AVenomObstacle> ObstacleClass;

	UPROPERTY(EditAnywhere, Category = "Obstacles")
	int32 NumObstacles = 7;

	UPROPERTY(EditAnywhere, Category = "Obstacles")
	float ObstacleMinRadius = 400.f;

	UPROPERTY(EditAnywhere, Category = "Obstacles")
	float ObstacleMaxRadius = 1150.f;

	// Seconds each wave lasts before the next begins.
	UPROPERTY(EditAnywhere, Category = "Wave")
	float WaveDuration = 25.f;

	FTimerHandle SpawnTimer;
	FTimerHandle WaveTimer;
	FTimerHandle HordeTimer;

	// --- Current wave configuration (set by StartWave) ---
	int32 WaveIndex = 0;

	bool bCurRunner = false;
	bool bCurJumper = false;
	bool bCurRanged = false;
	float CurRunnerChance = 0.f;
	float CurJumperChance = 0.f;
	float CurRangedChance = 0.f;
	float CurSpawnInterval = 1.6f;
	int32 CurMaxMobs = 14;
	bool bCurHorde = false;
	float CurHordeInterval = 12.f;
	int32 CurHordeSize = 8;
};
