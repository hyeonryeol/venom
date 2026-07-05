// venom — Player controller that ticks input while paused.

#include "VenomPlayerController.h"

AVenomPlayerController::AVenomPlayerController()
{
	// Protected on APlayerController, but reachable from this subclass.
	bShouldPerformFullTickWhenPaused = true;
}
