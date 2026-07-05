// venom — Player controller that keeps ticking (input) while the game is paused,
// so the level-up augment picker can read 1/2/3 during the pause.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VenomPlayerController.generated.h"

UCLASS()
class MCPGAMEPROJECT_API AVenomPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AVenomPlayerController();
};
