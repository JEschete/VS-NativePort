#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Items/EquipmentStats.hpp"
#include "Items/ItemRecords.hpp"

// Ashley's inventory: item queries and consumption, and the menus' item
// tables (slot occupancy, display order, copying and stacking).
namespace VS::Items {
inline constexpr std::size_t kItemIdCategoryCount = 8;

// How many of item `id` Ashley holds: 1 for a blade, grip, armour or gem she
// has, a stack's count for a misc item, and 0 otherwise (shields and ids past
// the last category are never held). Id 0 counts as held.
// `category_upper_ids` holds each id category's first id past it.
[[nodiscard]] std::int32_t InventoryItemCount(
    std::int32_t id,
    std::span<const std::uint32_t, kItemIdCategoryCount> category_upper_ids,
    const InventoryContents& inventory);

// Misc items from this id on are taken from the stack the menu invoked
// (`invoked_slot`) onward; earlier ids from the first matching stack.
inline constexpr std::int32_t kFirstInvokedMiscId = 0x143;

// Takes one of misc item `misc_id` (an emptied stack is cleared); false when
// Ashley holds none.
[[nodiscard]] bool ConsumeMiscItem(std::int32_t misc_id,
                                   std::uint8_t& invoked_slot,
                                   InventoryContents& inventory);
}  // namespace VS::Items

namespace VS::Items::Inventory {
// A misc item stack holds at most this many.
inline constexpr std::int32_t kMiscStackLimit = 100;
// CopyItem's flags: the category in bits 0-3, kCopyItemWrite to copy (without
// it the call only reports where the item would go), and the owning weapon
// or shield slot from bit 8 (a shield's carries kShieldGemFlag).
inline constexpr std::int32_t kCopyItemCategoryMask = 0xF;
inline constexpr std::int32_t kCopyItemWrite = 0x10;
inline constexpr int kCopyItemParentShift = 8;

// Ashley's equipped loadouts (decomp: vs_battle_actorStats).
struct EquippedLoadouts {
  WeaponLoadout weapon{};
  ShieldLoadout shield{};
  std::array<ArmorLoadout, kLimbCount> limb_armor{};
  AccessorySpawnLoadout accessory{};
};

// The 1-based slot a copy took (or would take); 0 when the target has no
// room. A misc item merged entirely into existing stacks reports slot 1.
struct ItemCopy {
  std::int32_t slot{};
  ItemsStatus status{ItemsStatus::kOk};
};

// Unequips Ashley's weapon (or shield) and clears every inventory weapon's
// (or shield's) equipped mark.
[[nodiscard]] ItemsStatus UnequipWeapon(AshleyWeaponState& ashley,
                                        InventoryContents& inventory,
                                        EquipStatusHost& host);
[[nodiscard]] ItemsStatus UnequipShield(ShieldLoadout& ashley_shield,
                                        InventoryContents& inventory,
                                        EquipStatusHost& host);
// Empties 1-based slot `slot` of `category`, with a weapon's blade, grip and
// gems and a shield's gems; the slot keeps its number. The manage row has no
// slots.
[[nodiscard]] ItemsStatus ClearItemSlot(ItemCategory category,
                                        std::int32_t slot,
                                        InventoryContents& inventory);
// The id that marks 0-based slot `index` of `category` occupied (a weapon's
// blade reference); 0 for an empty slot and the manage row; nothing for a
// slot outside the table.
[[nodiscard]] std::optional<std::uint16_t> ItemOccupancyId(
    ItemCategory category, std::size_t index,
    const InventoryContents& inventory);
// The display order of `category`; empty for the manage row.
[[nodiscard]] std::span<std::uint8_t> DisplayOrder(InventoryIndices& indices,
                                                   ItemCategory category);
// The 1-based display position of 0-based slot `slot`, or 0.
[[nodiscard]] std::int32_t FindDisplayPosition(
    std::span<const std::uint8_t> order, std::int32_t slot);
// Lists 0-based slot `slot` in the first free display position.
void AppendDisplayIndex(std::span<std::uint8_t> order, std::int32_t slot);
// Drops empty slots from the display order of `category`, closes the gaps
// and appends occupied slots it did not list. The manage row has no order.
[[nodiscard]] ItemsStatus RebuildDisplayIndices(
    ItemCategory category, InventoryIndices& indices,
    const InventoryContents& inventory);
[[nodiscard]] std::int32_t CountItems(ItemCategory category,
                                      const InventoryContents& inventory);
// The first empty 0-based slot, or the category's capacity when it is full.
[[nodiscard]] std::size_t FirstEmptySlot(ItemCategory category,
                                         const InventoryContents& inventory);
// Copies 0-based slot `source_index` of the flags' category from `source`
// into the first empty slot of `target`, with a weapon's or shield's parts.
// Misc items first fill matching stacks in the display order of `displayed`
// (which the copy rebuilds) up to kMiscStackLimit. A source slot outside its
// table is refused where the copy reads it.
[[nodiscard]] ItemCopy CopyItem(std::int32_t flags, InventoryContents& target,
                                std::int32_t source_index,
                                const InventoryContents& source,
                                InventoryIndices& displayed_indices,
                                const InventoryContents& displayed);
// Stores each equipped loadout's worn state back into its inventory slot.
[[nodiscard]] ItemsStatus StoreEquippedStats(EquippedLoadouts& ashley,
                                             InventoryContents& inventory);
}  // namespace VS::Items::Inventory
