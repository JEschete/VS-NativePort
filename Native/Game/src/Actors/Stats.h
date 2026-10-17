// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_STATS_H_TYPES
#define VS_ACTORS_STATS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ACTORS_STATS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ACTORS_STATS_H_DECLS)
#define VS_ACTORS_STATS_H_DECLS
#include "Battle/Actor.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"

Battle_LimbStats* Actors_CombatLimbAt(Battle_ActorStats* stats, int limb);

#endif // VS_ACTORS_STATS_H_DECLS
