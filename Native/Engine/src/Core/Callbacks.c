// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Battle/Actor.h"
#include "Core/Callbacks.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"

// decomp: BATTLE.PRG vs_battle_nop0 (src/BATTLE/BATTLE.PRG/146C.c:1354)
void Core_IgnoreCallback(Battle_ActorStats* actor __attribute__((unused))) { }

