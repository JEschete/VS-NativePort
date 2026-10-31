// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/Stats.h"
#include "Battle/Actor.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"

// decomp: BATTLE.PRG actorLimb (src/BATTLE/BATTLE.PRG/74088.c:1001)
Battle_LimbStats* Actors_CombatLimbAt(Battle_ActorStats* stats, int limb)
{
    return ((Battle_ActorStats*)(limb * sizeof(Battle_LimbStats) + (intptr_t)stats))
        ->limbs;
}

}  // extern "C"
