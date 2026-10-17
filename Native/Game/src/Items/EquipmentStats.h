// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ITEMS_EQUIPMENTSTATS_H_TYPES
#define VS_ITEMS_EQUIPMENTSTATS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ITEMS_EQUIPMENTSTATS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ITEMS_EQUIPMENTSTATS_H_DECLS)
#define VS_ITEMS_EQUIPMENTSTATS_H_DECLS
#include "Items/Equipment.h"
#include "Items/EquipmentInit.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"

void Items_BuildWeaponStatsFromParts(
    Items_WeaponLoadout* target, Items_WeaponSpawnLoadout* source);
void Items_BuildShieldStatsFromParts(
    Items_ShieldLoadout* target, Items_ShieldSpawnLoadout* source);
void Items_BuildAccessoryStatsFromPart(Items_AccessorySpawnLoadout* target, Items_AccessoryInit* source);
void Items_BuildArmorStatsFromPart(Items_ArmorLoadout* target, Items_ArmorInit* source);
void Items_ReadInventoryBladeStats(
    Items_EquipmentPartStats* target, Items_InventoryBlade* source);
void Items_ReadInventoryGripStats(
    Items_EquipmentPartStats* target, Items_InventoryGrip* source);
void Items_ReadInventoryArmorStats(
    Items_EquipmentPartStats* target, Items_InventoryArmor* source);
void Items_ReadInventoryGemStats(
    Items_EquipmentPartStats* target, Items_InventoryGem* source);
void Items_LoadInventoryWeaponStats(Items_WeaponLoadout* target, Items_InventoryWeapon* source);
void Items_LoadInventoryShieldStats(Items_ShieldLoadout* target, Items_InventoryShield* source);
void Items_LoadInventoryArmorStats(Items_ArmorLoadout* target, Items_InventoryArmor* source);
void Items_LoadInventoryAccessoryStats(
    Items_AccessorySpawnLoadout* target, Items_InventoryArmor* source);
void Items_RemoveEquipmentBoostOnEquip(void);
void Items_RemoveWeaponEnchantmentOnEquip(void);
void Items_RemoveArmorEnchantmentOnEquip(void);
void Items_EquipAshleyWeapon(Items_InventoryWeapon* weapon);
void Items_EquipAshleyShield(Items_InventoryShield* shield);
void Items_EquipAshleyArmor(int bodyPart, Items_InventoryArmor* armor);
void Items_EquipAshleyAccessory(Items_InventoryArmor* accessory);
void Items_WriteInventoryBladeStats(
    Items_InventoryBlade* target, Items_EquipmentPartStats* source);
void Items_WriteInventoryGripStats(
    Items_InventoryGrip* target, Items_EquipmentPartStats* source);
void Items_WriteInventoryGemStats(Items_InventoryGem* target, Items_EquipmentPartStats* source);
void Items_WriteInventoryArmorStats(
    Items_InventoryArmor* target, Items_EquipmentPartStats* source);
void Items_StoreInventoryWeaponStats(
    Items_InventoryWeapon* target, Items_WeaponLoadout* source);
void Items_StoreInventoryShieldStats(
    Items_InventoryShield* target, Items_ShieldLoadout* source);
void Items_StoreInventoryArmorStats(Items_InventoryArmor* target, Items_ArmorLoadout* source);
void Items_StoreInventoryAccessoryStats(
    Items_InventoryArmor* target, Items_AccessorySpawnLoadout* source);

#endif // VS_ITEMS_EQUIPMENTSTATS_H_DECLS
