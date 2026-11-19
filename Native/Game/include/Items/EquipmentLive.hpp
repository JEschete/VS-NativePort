#pragma once
// Hand-maintained (tools/port/owned.csv).
#include <cstdint>

#include "Items/ItemRecords.hpp"

struct Battle_ActionDefinition;
struct Battle_ActorStats;
struct Battle_HitResult;
struct Items_AccessorySpawnLoadout;
struct Items_ArmorLoadout;
struct Items_ShieldLoadout;
struct Items_ShieldSpawnLoadout;
struct Items_WeaponLoadout;
struct Items_WeaponSpawnLoadout;

// Bindings from the battle actor records, Ashley's battle actor and the
// inventory (VS::Items::inventory) to the typed equipment statistics,
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
                                     const VS::Items::AccessoryInit* source);
void BuildLiveArmorStatsFromPart(Items_ArmorLoadout* target,
                                 const VS::Items::ArmorInit* source);

void ReadLiveInventoryBladeStats(VS::Items::EquipmentPart* target,
                                 const VS::Items::InventoryBlade* source);
void ReadLiveInventoryGripStats(VS::Items::EquipmentPart* target,
                                const VS::Items::InventoryGrip* source);
void ReadLiveInventoryArmorStats(VS::Items::EquipmentPart* target,
                                 const VS::Items::InventoryArmor* source);
void ReadLiveInventoryGemStats(VS::Items::EquipmentPart* target,
                               const VS::Items::InventoryGem* source);

// The part references name slots of Ashley's inventory.
void LoadLiveInventoryWeaponStats(Items_WeaponLoadout* target,
                                  const VS::Items::InventoryWeapon* source);
void LoadLiveInventoryShieldStats(Items_ShieldLoadout* target,
                                  const VS::Items::InventoryShield* source);
void LoadLiveInventoryArmorStats(Items_ArmorLoadout* target,
                                 const VS::Items::InventoryArmor* source);
void LoadLiveInventoryAccessoryStats(Items_AccessorySpawnLoadout* target,
                                     const VS::Items::InventoryArmor* source);

// Equip Ashley (Battle_characterState) from her inventory.
void EquipLiveAshleyWeapon(const VS::Items::InventoryWeapon* weapon);
void EquipLiveAshleyShield(const VS::Items::InventoryShield* shield);
void EquipLiveAshleyArmor(int body_part,
                          const VS::Items::InventoryArmor* armor);
void EquipLiveAshleyAccessory(const VS::Items::InventoryArmor* accessory);

void WriteLiveInventoryBladeStats(VS::Items::InventoryBlade* target,
                                  const VS::Items::EquipmentPart* source);
void WriteLiveInventoryGripStats(VS::Items::InventoryGrip* target,
                                 const VS::Items::EquipmentPart* source);
void WriteLiveInventoryGemStats(VS::Items::InventoryGem* target,
                                const VS::Items::EquipmentPart* source);
void WriteLiveInventoryArmorStats(VS::Items::InventoryArmor* target,
                                  const VS::Items::EquipmentPart* source);

// Store a loadout back into `target` and the parts it references in
// Ashley's inventory.
void StoreLiveInventoryWeaponStats(VS::Items::InventoryWeapon* target,
                                   Items_WeaponLoadout* source);
void StoreLiveInventoryAccessoryStats(
    VS::Items::InventoryArmor* target,
    const Items_AccessorySpawnLoadout* source);

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
