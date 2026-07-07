// venom — Game mode: makes the parasite the player pawn and spawns mobs.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VenomGameMode.generated.h"

class AMobEnemy;

UCLASS()
class MCPGAMEPROJECT_API AVenomGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVenomGameMode();

protected:
	virtual void BeginPlay() override;

	/** Spawns one mob on a ring around the player, up to MaxMobs alive. */
	void SpawnMob();

	UPROPERTY(EditAnywhere, Category = "Spawn")
	TSubclassOf<AMobEnemy> MobClass;

	UPROPERTY(EditAnywhere, Category = "Spawn")
	TSubclassOf<AMobEnemy> RangedMobClass;

	// Chance each spawn is a ranged goblin.
	UPROPERTY(EditAnywhere, Category = "Spawn")
	float RangedChance = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Spawn")
	float SpawnInterval = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Spawn")
	float SpawnRadius = 1500.f;

	UPROPERTY(EditAnywhere, Category = "Spawn")
	int32 MaxMobs = 30;

	FTimerHandle SpawnTimer;
};
