// Compile-time checks of the x64 layout decisions the copy depends on (tools/port/copy_fixups.py TYPE_FIXES and
// FIXES). A failure here means a fix no longer holds after a ledger or decomp change.
#include "Legacy/Psx.h"
#include "Actors/ActorLoading.h"
#include "Actors/ActorRecord.h"
#include "Actors/ModelRendering.h"
#include "Battle/Actor.h"

#include <stddef.h>

// An actor record placed at core.model (Content_ParseActorModel) must end before the weapon model slots.
_Static_assert(offsetof(Actors_ActorAllocation, modelSlots) - offsetof(Actors_ActorAllocation, core.model) >=
                   sizeof(Actors_ActorRecord),
    "the actor record overlaps the weapon model slots");

// Actors_SpawnRoomEnemy: `actor + 1` is the stats block of the allocation.
_Static_assert(offsetof(Actors_ActorAllocation, core.actor.stats) == sizeof(Battle_Actor),
    "stats do not follow the Battle_Actor");

// On-disc structs keep the file layout (ONDISC_OFFSETS): the SHP header is 64 bytes, the bones follow.
_Static_assert(sizeof(Actors_ShpModel) == 64, "SHP header layout");
_Static_assert(offsetof(Actors_ShpModel, bones) == 64, "SHP bones offset");
_Static_assert(offsetof(Actors_SeqAnimationBank, headers) == 8, "SEQ bank header layout");

int Tests_LayoutAssertsLinked(void) { return 1; }
