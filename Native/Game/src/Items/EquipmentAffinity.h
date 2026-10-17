// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ITEMS_EQUIPMENTAFFINITY_H_TYPES
#define VS_ITEMS_EQUIPMENTAFFINITY_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ITEMS_EQUIPMENTAFFINITY_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ITEMS_EQUIPMENTAFFINITY_H_DECLS)
#define VS_ITEMS_EQUIPMENTAFFINITY_H_DECLS
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"

void Items_RebuildArmorClassAffinity(Battle_ActorStats* arg0, int arg1);
void Items_RebuildShieldClassAffinity(Battle_ActorStats* arg0);
void Items_RebuildWeaponClassAffinity(Battle_ActorStats* arg0);
void Items_GrowOpposingEquipmentAffinity(Battle_ActionDefinition* action, Items_EquipmentPartStats* equipment,
    int affinity, int arg3, int actorId);
void Items_GrowMatchingEquipmentAffinity(Battle_ActionDefinition* action, Items_EquipmentPartStats* equipment,
    int affinity, int arg3, int suppressNotification);
void Items_GrowEquipmentClass(Battle_ActionDefinition* action __attribute__((unused)),
    Items_EquipmentPartStats* equipment, int class, int arg3, int actorId);
void Items_GrowDefenderEquipmentRatings(
    Battle_ActionDefinition* arg0, Battle_ActorStats* arg1, int arg2, int arg3, int arg4);
void Items_GrowAttackerWeaponRatings(Battle_ActionDefinition* action, Battle_ActorStats* arg1, int arg2, int arg3);

#endif // VS_ITEMS_EQUIPMENTAFFINITY_H_DECLS
