// venom — On-screen HUD. Draws the player's health bar.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "VenomHUD.generated.h"

UCLASS()
class MCPGAMEPROJECT_API AVenomHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
