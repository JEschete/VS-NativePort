#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <span>

#include "Items/ItemRecords.hpp"

// Equipment statistics: building an actor's weapon, shield, armour and
// accessory loadouts from part records, moving parts between the inventory
// tables and loadouts, and equipping Ashley. The functions work on the typed
// records of ItemRecords.hpp; the status effects that equipping clears are
// reached through EquipStatusHost.
namespace VS::Items {
// Builds a weapon loadout from its blade, grip and gems. A blade damage type
// past the grip's type, class and affinity tables leaves damage_type_value 0
// and returns kDamageTypeOutOfRange.
[[nodiscard]] ItemsStatus BuildWeaponStatsFromParts(
    WeaponLoadout& target, const WeaponSpawnLoadout& source);
void BuildShieldStatsFromParts(ShieldLoadout& target,
                               const ShieldSpawnLoadout& source);
void BuildAccessoryStatsFromPart(AccessorySpawnLoadout& target,
                                 const AccessoryInit& source);
void BuildArmorStatsFromPart(ArmorLoadout& target, const ArmorInit& source);

// Copy an inventory record's fields into a loadout part. Fields the record
// does not carry keep their values.
void ReadInventoryBladeStats(EquipmentPart& target,
                             const InventoryBlade& source);
void ReadInventoryGripStats(EquipmentPart& target, const InventoryGrip& source);
void ReadInventoryArmorStats(EquipmentPart& target,
                             const InventoryArmor& source);
void ReadInventoryGemStats(EquipmentPart& target, const InventoryGem& source);

// Rebuild a loadout from an inventory record and the parts it references in
// `inventory`; no record builds the empty loadout. A part reference outside
// its table leaves that part empty and returns kPartReferenceOutOfRange.
[[nodiscard]] ItemsStatus LoadInventoryWeaponStats(
    WeaponLoadout& target, const InventoryWeapon* source,
    const InventoryContents& inventory);
[[nodiscard]] ItemsStatus LoadInventoryShieldStats(
    ShieldLoadout& target, const InventoryShield* source,
    const InventoryContents& inventory);
void LoadInventoryArmorStats(ArmorLoadout& target,
                             const InventoryArmor* source);
void LoadInventoryAccessoryStats(AccessorySpawnLoadout& target,
                                 const InventoryArmor* source);

// Ashley's status effects that a change of equipment ends.
class EquipStatusHost {
 public:
  EquipStatusHost() = default;
  EquipStatusHost(const EquipStatusHost&) = delete;
  EquipStatusHost& operator=(const EquipStatusHost&) = delete;
  EquipStatusHost(EquipStatusHost&&) = delete;
  EquipStatusHost& operator=(EquipStatusHost&&) = delete;
  virtual ~EquipStatusHost() = default;

  // The equip functions call the host after they have written the new
  // equipment: a host that keeps statuses next to the equipment applies the
  // equipment before it answers or removes anything.
  [[nodiscard]] virtual std::int32_t AshleyStatuses() = 0;
  // Queues the "status ended" labels for `statuses` over Ashley.
  virtual void QueueStatusLabels(std::int32_t statuses) = 0;
  virtual void RemoveStatuses(std::int32_t statuses) = 0;
};

// Status masks that equipping ends.
inline constexpr std::int32_t kEquipmentBoostStatus = 0x100000;
inline constexpr std::int32_t kWeaponEnchantmentStatuses = 0x01E00000;
inline constexpr std::int32_t kArmorEnchantmentStatuses = 0x1E000000;

void RemoveEquipmentBoostOnEquip(EquipStatusHost& host);
void RemoveWeaponEnchantmentOnEquip(EquipStatusHost& host);
void RemoveArmorEnchantmentOnEquip(EquipStatusHost& host);

// Ashley's weapon and the reach fields equipping it derives.
struct AshleyWeaponState {
  WeaponLoadout weapon{};
  // Ashley's 5-bit reach bonus.
  std::uint8_t reach{};
  std::uint8_t equipped_weapon_category{};
  std::uint8_t current_range{};
  std::uint8_t current_range_y{};
  std::uint8_t current_range_z{};
  std::uint8_t current_attack_shape{};
  std::uint8_t current_attack_shape_angle{};
};

// Weapon category of an unarmed actor.
inline constexpr std::uint8_t kUnarmedWeaponCategory = 10;

// Equip `weapon` (none: unarmed), then end the boost and weapon enchantments.
// A blade reference outside the table keeps the previous weapon category.
[[nodiscard]] ItemsStatus EquipAshleyWeapon(AshleyWeaponState& ashley,
                                            const InventoryWeapon* weapon,
                                            const InventoryContents& inventory,
                                            EquipStatusHost& host);
[[nodiscard]] ItemsStatus EquipAshleyShield(ShieldLoadout& target,
                                            const InventoryShield* shield,
                                            const InventoryContents& inventory,
                                            EquipStatusHost& host);
// `limb_armor` holds each limb's armour; a body part outside it changes
// nothing and returns kBodyPartOutOfRange.
[[nodiscard]] ItemsStatus EquipAshleyArmor(std::span<ArmorLoadout> limb_armor,
                                           std::int32_t body_part,
                                           const InventoryArmor* armor,
                                           EquipStatusHost& host);
void EquipAshleyAccessory(AccessorySpawnLoadout& target,
                          const InventoryArmor* accessory,
                          EquipStatusHost& host);

// Copy a loadout part's fields back into an inventory record. The record's
// slot and owner fields keep their values.
void WriteInventoryBladeStats(InventoryBlade& target,
                              const EquipmentPart& source);
void WriteInventoryGripStats(InventoryGrip& target,
                             const EquipmentPart& source);
void WriteInventoryGemStats(InventoryGem& target, const EquipmentPart& source);
void WriteInventoryArmorStats(InventoryArmor& target,
                              const EquipmentPart& source);

// Store a loadout's worn state back into the inventory. The weapon and
// shield also carry their current DP and PP into the part records first. A
// part reference outside its table skips that part and returns
// kPartReferenceOutOfRange.
[[nodiscard]] ItemsStatus StoreInventoryWeaponStats(
    InventoryWeapon& target, WeaponLoadout& source,
    InventoryContents& inventory);
[[nodiscard]] ItemsStatus StoreInventoryShieldStats(
    InventoryShield& target, ShieldLoadout& source,
    InventoryContents& inventory);
void StoreInventoryArmorStats(InventoryArmor& target, ArmorLoadout& source);
void StoreInventoryAccessoryStats(InventoryArmor& target,
                                  const AccessorySpawnLoadout& source);
}  // namespace VS::Items
