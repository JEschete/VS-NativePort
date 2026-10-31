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

void Items_RebuildArmorClassAffinity(Battle_ActorStats* actor_stats, int limb_index);
void Items_RebuildShieldClassAffinity(Battle_ActorStats* actor_stats);
void Items_RebuildWeaponClassAffinity(Battle_ActorStats* actor_stats);
void Items_GrowOpposingEquipmentAffinity(Battle_ActionDefinition* action, Items_EquipmentPartStats* equipment,
    int affinity, int equipment_label_id, int actorId);
void Items_GrowMatchingEquipmentAffinity(Battle_ActionDefinition* action, Items_EquipmentPartStats* equipment,
    int affinity, int equipment_label_id, int suppressNotification);
void Items_GrowEquipmentClass(Battle_ActionDefinition* action __attribute__((unused)),
    Items_EquipmentPartStats* equipment, int class_, int equipment_label_id, int actorId);
void Items_GrowDefenderEquipmentRatings(
    Battle_ActionDefinition* action, Battle_ActorStats* defender_stats, int attacker_class_id, int attacker_affinity_id, int hit_limb_index);
void Items_GrowAttackerWeaponRatings(Battle_ActionDefinition* action, Battle_ActorStats* attacker_stats, int defender_class_id, int defender_affinity_id);

#endif // VS_ITEMS_EQUIPMENTAFFINITY_H_DECLS
