#pragma once
// Hand-maintained (tools/port/owned.csv).
#include <cstdint>

struct Battle_ActionDefinition;
struct Battle_ActorStats;
struct Battle_HitResult;
struct Items_AccessoryInit;
struct Items_AccessorySpawnLoadout;
struct Items_ArmorInit;
struct Items_ArmorLoadout;
struct Items_EquipmentPartStats;
struct Items_InventoryArmor;
struct Items_InventoryBlade;
struct Items_InventoryGem;
struct Items_InventoryGrip;
struct Items_InventoryShield;
struct Items_InventoryWeapon;
struct Items_ShieldLoadout;
struct Items_ShieldSpawnLoadout;
struct Items_WeaponLoadout;
struct Items_WeaponSpawnLoadout;

// Bindings from the battle actor records, Ashley's battle actor and the
// inventory (Audio_Seq_unusedInventory) to the typed equipment statistics,
// growth, wear and recovery rules (EquipmentStats.hpp, EquipmentAffinity.hpp,
// EquipmentWear.hpp, Recovery.hpp), in the shapes the copied callers use.
// Each reports a refusal of the typed rule with a diagnostic; a null record,
// a missing Ashley or an actor id past the battle actor table changes nothing.
namespace VS::Items {
void BuildLiveWeaponStatsFromParts(Items_WeaponLoadout* target,
                                   const Items_WeaponSpawnLoadout* source);
void BuildLiveShieldStatsFromParts(Items_ShieldLoadout* target,
                                   const Items_ShieldSpawnLoadout* source);
void BuildLiveAccessoryStatsFromPart(Items_AccessorySpawnLoadout* target,
                                     const Items_AccessoryInit* source);
void BuildLiveArmorStatsFromPart(Items_ArmorLoadout* target,
                                 const Items_ArmorInit* source);

void ReadLiveInventoryBladeStats(Items_EquipmentPartStats* target,
                                 const Items_InventoryBlade* source);
void ReadLiveInventoryGripStats(Items_EquipmentPartStats* target,
                                const Items_InventoryGrip* source);
void ReadLiveInventoryArmorStats(Items_EquipmentPartStats* target,
                                 const Items_InventoryArmor* source);
void ReadLiveInventoryGemStats(Items_EquipmentPartStats* target,
                               const Items_InventoryGem* source);

// The part references name slots of Ashley's inventory.
void LoadLiveInventoryWeaponStats(Items_WeaponLoadout* target,
                                  const Items_InventoryWeapon* source);
void LoadLiveInventoryShieldStats(Items_ShieldLoadout* target,
                                  const Items_InventoryShield* source);
void LoadLiveInventoryArmorStats(Items_ArmorLoadout* target,
                                 const Items_InventoryArmor* source);
void LoadLiveInventoryAccessoryStats(Items_AccessorySpawnLoadout* target,
                                     const Items_InventoryArmor* source);

// Equip Ashley (Battle_characterState) from her inventory.
void EquipLiveAshleyWeapon(const Items_InventoryWeapon* weapon);
void EquipLiveAshleyShield(const Items_InventoryShield* shield);
void EquipLiveAshleyArmor(int body_part, const Items_InventoryArmor* armor);
void EquipLiveAshleyAccessory(const Items_InventoryArmor* accessory);

void WriteLiveInventoryBladeStats(Items_InventoryBlade* target,
                                  const Items_EquipmentPartStats* source);
void WriteLiveInventoryGripStats(Items_InventoryGrip* target,
                                 const Items_EquipmentPartStats* source);
void WriteLiveInventoryGemStats(Items_InventoryGem* target,
                                const Items_EquipmentPartStats* source);
void WriteLiveInventoryArmorStats(Items_InventoryArmor* target,
                                  const Items_EquipmentPartStats* source);

// Store a loadout back into `target` and the parts it references in
// Ashley's inventory.
void StoreLiveInventoryWeaponStats(Items_InventoryWeapon* target,
                                   Items_WeaponLoadout* source);
void StoreLiveInventoryAccessoryStats(
    Items_InventoryArmor* target, const Items_AccessorySpawnLoadout* source);

void RebuildLiveArmorClassAffinity(Battle_ActorStats* actor_stats,
                                   int limb_index);
void RebuildLiveShieldClassAffinity(Battle_ActorStats* actor_stats);
void RebuildLiveWeaponClassAffinity(Battle_ActorStats* actor_stats);

// The defender's weapon-drawn state comes from Battle_actors and each limb's
// armour loss from Battle_limbArmorDpLoss.
void ConsumeLiveHitEquipmentDp(const Battle_ActionDefinition* action,
                               Battle_ActorStats* source,
                               Battle_ActorStats* target, int amount);
void AccumulateLiveHitEquipmentPp(const Battle_ActionDefinition* action,
                                  Battle_ActorStats* source_stats,
                                  Battle_ActorStats* target_stats,
                                  int dp_amount);
void GrowLiveDefenderEquipmentRatings(const Battle_ActionDefinition* action,
                                      Battle_ActorStats* defender_stats,
                                      int attacker_class_id,
                                      int attacker_affinity_id,
                                      int hit_limb_index);
void GrowLiveAttackerWeaponRatings(const Battle_ActionDefinition* action,
                                   Battle_ActorStats* attacker_stats,
                                   int defender_class_id,
                                   int defender_affinity_id);

// BATTLE.PRG stores this address in its stat calculator table
// (Battle_statCalculators[15]), so it keeps the table's signature. A hit index
// outside the action's hits recovers nothing.
std::int16_t CalculateLiveItemRecovery(Battle_ActionDefinition* action,
                                       Battle_HitResult* attacker_result,
                                       Battle_HitResult* target_result,
                                       int hit_index, int add_random_variation);
}  // namespace VS::Items
