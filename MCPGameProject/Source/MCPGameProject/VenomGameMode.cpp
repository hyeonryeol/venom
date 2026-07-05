// venom — Game mode: makes the parasite the player pawn.

#include "VenomGameMode.h"
#include "ParasitePawn.h"

AVenomGameMode::AVenomGameMode()
{
	DefaultPawnClass = AParasitePawn::StaticClass();
}
