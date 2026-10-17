// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ITEMS_EQUIPMENTWEAR_H_TYPES
#define VS_ITEMS_EQUIPMENTWEAR_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ITEMS_EQUIPMENTWEAR_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ITEMS_EQUIPMENTWEAR_H_DECLS)
#define VS_ITEMS_EQUIPMENTWEAR_H_DECLS
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"

short Items_ApplyPpGemMultiplier(int arg0, short arg1, char* arg2);
void Items_ConsumeHitEquipmentDp(
    Battle_ActionDefinition* action, Battle_ActorStats* source, Battle_ActorStats* target, int amount);
void Items_AccumulateHitEquipmentPp(
    Battle_ActionDefinition* action, Battle_ActorStats* arg1, Battle_ActorStats* arg2, int arg3);

#endif // VS_ITEMS_EQUIPMENTWEAR_H_DECLS
