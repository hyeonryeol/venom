// venom — On-screen HUD. Player health bar + ARAM-style augment cards.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "VenomHUD.generated.h"

class AParasitePawn;

UCLASS()
class MCPGAMEPROJECT_API AVenomHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	virtual void NotifyHitBoxClick(FName BoxName) override;
	virtual void NotifyHitBoxBeginCursorOver(FName BoxName) override;
	virtual void NotifyHitBoxEndCursorOver(FName BoxName) override;

protected:
	void DrawHealthBar(AParasitePawn* Player);
	void DrawAugmentCards(AParasitePawn* Player);

	// Which card the mouse is over (NAME_None when none).
	FName HoveredBox;
};
