// Hand-maintained (tools/port/owned.csv).
#include "Items/Inventory.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Items/EquipmentStats.hpp"
#include "Items/ItemRecords.hpp"

namespace VS::Items {
namespace {
// The id categories InventoryItemCount searches (decomp: _itemCategorySizes);
// category 3's ids are not searched for.
constexpr std::size_t kBladeIds = 0;
constexpr std::size_t kGripIds = 1;
constexpr std::size_t kArmorIds = 2;
constexpr std::size_t kGemIds = 4;
constexpr std::size_t kFirstMiscIds = 5;

template <typename Table>
bool HoldsId(const Table& table, std::int32_t id) {
  return std::any_of(table.begin(), table.end(),
                     [id](const auto& item) { return item.id == id; });
}

std::int32_t MiscCount(const InventoryContents& inventory, std::int32_t id) {
  const auto stack = std::find_if(inventory.misc.begin(), inventory.misc.end(),
                                  [id](const InventoryMisc& misc) {
                                    return misc.id == id && misc.count != 0;
                                  });
  return stack == inventory.misc.end() ? 0 : stack->count;
}
}  // namespace

// decomp: BATTLE.PRG vs_battle_itemIdIsInInventory
// (src/BATTLE/BATTLE.PRG/146C.c:9646)
// The original compares the id unsigned, so a negative id is past every
// category.
std::int32_t InventoryItemCount(
    std::int32_t id,
    std::span<const std::uint32_t, kItemIdCategoryCount> category_upper_ids,
    const InventoryContents& inventory) {
  if (id == 0) return 1;
  const auto unsigned_id = static_cast<std::uint32_t>(id);
  const auto upper = std::find_if(
      category_upper_ids.begin(), category_upper_ids.end(),
      [unsigned_id](std::uint32_t bound) { return unsigned_id < bound; });
  const auto category =
      static_cast<std::size_t>(upper - category_upper_ids.begin());
  switch (category) {
    case kBladeIds:
      return HoldsId(inventory.blades, id) ? 1 : 0;
    case kGripIds:
      return HoldsId(inventory.grips, id) ? 1 : 0;
    case kArmorIds:
      return HoldsId(inventory.armor, id) ? 1 : 0;
    case kGemIds:
      return HoldsId(inventory.gems, id) ? 1 : 0;
    default:
      break;
  }
  if (category < kFirstMiscIds || category >= kItemIdCategoryCount) return 0;
  return MiscCount(inventory, id);
}

// decomp: BATTLE.PRG vs_battle_decreaseMiscCount
// (src/BATTLE/BATTLE.PRG/5BF94.c:4454)
bool ConsumeMiscItem(std::int32_t misc_id, std::uint8_t& invoked_slot,
                     InventoryContents& inventory) {
  if (misc_id < kFirstInvokedMiscId) invoked_slot = 0;
  for (std::size_t i = 0; i < kMiscSlots; ++i) {
    auto& stack = inventory.misc[(invoked_slot + i) % kMiscSlots];
    if (stack.id != misc_id) continue;
    // An empty stack wraps to 255 and keeps its id, as in the original.
    const std::int32_t remaining = stack.count - 1;
    stack.count = static_cast<std::uint8_t>(remaining);
    if (remaining == 0) stack.id = 0;
    return true;
  }
  return false;
}
}  // namespace VS::Items

namespace VS::Items::Inventory {
namespace {
constexpr std::size_t CategoryIndex(ItemCategory category) {
  return static_cast<std::size_t>(category);
}

// The record a 0-based slot index names, or null outside the table.
template <typename Table>
auto* IndexedSlot(Table& table, std::int32_t index) {
  return ReferencedSlot(table, static_cast<std::uint32_t>(index) + 1U);
}

template <typename Table>
std::optional<std::uint16_t> OccupancyAt(const Table& table, std::size_t index,
                                         auto field) {
  if (index >= table.size()) return std::nullopt;
  return static_cast<std::uint16_t>(field(table[index]));
}

// Empties 1-based slot `slot`, which keeps its number.
template <typename Record, std::size_t N>
ItemsStatus ResetSlot(std::array<Record, N>& table, std::int32_t slot) {
  auto* record = ReferencedSlot(table, static_cast<std::uint32_t>(slot));
  if (record == nullptr) return ItemsStatus::kSlotOutOfRange;
  *record = Record{};
  record->index = static_cast<decltype(record->index)>(slot);
  return ItemsStatus::kOk;
}

// Clears the gems a weapon or shield holds in its first three gem slots.
ItemsStatus ResetSetGems(std::span<const std::uint8_t, kShieldGemSlots> gems,
                         InventoryContents& inventory) {
  auto status = ItemsStatus::kOk;
  for (std::size_t i = 0; i < kWeaponGemCount; ++i) {
    if (gems[i] != 0) {
      KeepFirstRefusal(status, ResetSlot(inventory.gems, gems[i]));
    }
  }
  return status;
}

ItemsStatus ClearWeaponSlot(std::int32_t slot, InventoryContents& inventory) {
  const auto* weapon =
      ReferencedSlot(inventory.weapons, static_cast<std::uint32_t>(slot));
  if (weapon == nullptr) return ItemsStatus::kSlotOutOfRange;
  auto status = ResetSlot(inventory.blades, weapon->blade);
  KeepFirstRefusal(status, ResetSlot(inventory.grips, weapon->grip));
  KeepFirstRefusal(status, ResetSetGems(weapon->gems, inventory));
  KeepFirstRefusal(status, ResetSlot(inventory.weapons, slot));
  return status;
}

ItemsStatus ClearShieldSlot(std::int32_t slot, InventoryContents& inventory) {
  const auto* shield =
      ReferencedSlot(inventory.shields, static_cast<std::uint32_t>(slot));
  if (shield == nullptr) return ItemsStatus::kSlotOutOfRange;
  auto status = ResetSetGems(shield->gems, inventory);
  KeepFirstRefusal(status, ResetSlot(inventory.shields, slot));
  return status;
}

std::int32_t CountSetGems(std::span<const std::uint8_t, kShieldGemSlots> gems) {
  return static_cast<std::int32_t>(
      std::count_if(gems.begin(), gems.begin() + kWeaponGemCount,
                    [](std::uint8_t gem) { return gem != 0; }));
}

// One CopyItem call: its flags decoded and the target's state.
struct CopyRequest {
  std::int32_t parent{};
  bool write{};
  std::int32_t source_index{};
  std::int32_t item_count{};
  std::size_t empty_slot{};
  [[nodiscard]] std::int32_t Slot() const {
    return static_cast<std::int32_t>(empty_slot) + 1;
  }
};

// The flags that copy a weapon's or shield's part for `parent_slot`.
constexpr std::int32_t PartFlags(ItemCategory category,
                                 std::int32_t parent_slot) {
  return (parent_slot << kCopyItemParentShift) |
         static_cast<std::int32_t>(category) | kCopyItemWrite;
}

constexpr auto kGemLimit = static_cast<std::int32_t>(kGemSlots);

// Blades, grips, armour and gems copy as one record.
template <typename Record, std::size_t N, typename SetParent>
ItemCopy CopyPart(const CopyRequest& request, std::array<Record, N>& target,
                  const std::array<Record, N>& source, SetParent set_parent) {
  if (request.item_count == static_cast<std::int32_t>(N)) return {};
  if (!request.write) return {request.Slot()};
  const auto* from = IndexedSlot(source, request.source_index);
  if (from == nullptr) return {0, ItemsStatus::kSlotOutOfRange};
  auto& to = target[request.empty_slot];
  to = *from;
  to.index = static_cast<decltype(to.index)>(request.Slot());
  set_parent(to, request.parent);
  return {request.Slot()};
}

// The tables a weapon or shield copies its parts between.
struct PartTables {
  InventoryContents* target;
  const InventoryContents* source;
  InventoryIndices* displayed_indices;
  const InventoryContents* displayed;

  std::uint8_t CopyPartOf(std::int32_t flags, std::uint8_t reference,
                          ItemsStatus& status) const {
    const auto copy = CopyItem(flags, *target, reference - 1, *source,
                               *displayed_indices, *displayed);
    KeepFirstRefusal(status, copy.status);
    return static_cast<std::uint8_t>(copy.slot);
  }
};

ItemCopy CopyWeapon(const CopyRequest& request, const PartTables& tables) {
  auto& target = *tables.target;
  if (request.item_count == static_cast<std::int32_t>(kWeaponSlots) ||
      CountItems(ItemCategory::kBlade, target) ==
          static_cast<std::int32_t>(kBladeSlots) ||
      CountItems(ItemCategory::kGrip, target) ==
          static_cast<std::int32_t>(kGripSlots)) {
    return {};
  }
  const auto* from = IndexedSlot(tables.source->weapons, request.source_index);
  if (from == nullptr) return {0, ItemsStatus::kSlotOutOfRange};
  if (CountItems(ItemCategory::kGem, target) + CountSetGems(from->gems) >
      kGemLimit) {
    return {};
  }
  const auto slot = request.Slot();
  if (!request.write) return {slot};
  auto& to = target.weapons[request.empty_slot];
  to = InventoryWeapon{};
  to.index = static_cast<std::uint8_t>(slot);
  auto status = ItemsStatus::kOk;
  to.blade = tables.CopyPartOf(PartFlags(ItemCategory::kBlade, slot),
                               from->blade, status);
  to.grip = tables.CopyPartOf(PartFlags(ItemCategory::kGrip, slot), from->grip,
                              status);
  for (std::size_t i = 0; i < kWeaponGemCount; ++i) {
    if (from->gems[i] == 0) continue;
    to.gems[i] = tables.CopyPartOf(PartFlags(ItemCategory::kGem, slot),
                                   from->gems[i], status);
  }
  to.name = from->name;
  return {slot, status};
}

ItemCopy CopyShield(const CopyRequest& request, const PartTables& tables) {
  auto& target = *tables.target;
  if (request.item_count == static_cast<std::int32_t>(kShieldSlots)) return {};
  const auto* from = IndexedSlot(tables.source->shields, request.source_index);
  if (from == nullptr) return {0, ItemsStatus::kSlotOutOfRange};
  if (CountItems(ItemCategory::kGem, target) + CountSetGems(from->gems) >
      kGemLimit) {
    return {};
  }
  const auto slot = request.Slot();
  if (!request.write) return {slot};
  auto& to = target.shields[request.empty_slot];
  to = InventoryShield{};
  to.base = from->base;
  to.index = static_cast<std::uint8_t>(slot);
  auto status = ItemsStatus::kOk;
  const auto gem_flags = PartFlags(
      ItemCategory::kGem, slot | static_cast<std::int32_t>(kShieldGemFlag));
  for (std::size_t i = 0; i < kWeaponGemCount; ++i) {
    if (from->gems[i] == 0) continue;
    to.gems[i] = tables.CopyPartOf(gem_flags, from->gems[i], status);
  }
  return {slot, status};
}

// Adds `count` to `stack` up to kMiscStackLimit; returns what is left.
std::int32_t FillStack(InventoryMisc& stack, std::int32_t count) {
  if (kMiscStackLimit - stack.count >= count) {
    stack.count = static_cast<std::uint8_t>(stack.count + count);
    return 0;
  }
  const auto left = count + (stack.count - kMiscStackLimit);
  stack.count = static_cast<std::uint8_t>(kMiscStackLimit);
  return left;
}

ItemCopy CopyMisc(const CopyRequest& request, const PartTables& tables) {
  auto& target = *tables.target;
  const auto* from = IndexedSlot(tables.source->misc, request.source_index);
  if (from == nullptr) return {0, ItemsStatus::kSlotOutOfRange};
  std::int32_t count = from->count;
  auto status = ItemsStatus::kOk;
  // Probe before merging: a claim with no room keeps its whole stack.
  if (request.write) {
    const auto probe = CopyItem(static_cast<std::int32_t>(ItemCategory::kMisc),
                                target, request.source_index, *tables.source,
                                *tables.displayed_indices, *tables.displayed);
    if (probe.slot == 0) return {0, probe.status};
    status = probe.status;
  }
  KeepFirstRefusal(status, RebuildDisplayIndices(ItemCategory::kMisc,
                                                 *tables.displayed_indices,
                                                 *tables.displayed));
  for (const auto entry : tables.displayed_indices->misc) {
    auto* stack = ReferencedSlot(target.misc, entry);
    if (stack == nullptr || stack->id != from->id) continue;
    count = request.write
                ? FillStack(*stack, count)
                : std::max(count - (kMiscStackLimit - stack->count), 0);
  }
  if (count == 0) return {1, status};
  if (request.item_count == static_cast<std::int32_t>(kMiscSlots)) {
    return {0, status};
  }
  if (request.write) {
    auto& to = target.misc[request.empty_slot];
    to.id = from->id;
    to.count = static_cast<std::uint8_t>(count);
  }
  return {request.Slot(), status};
}
}  // namespace

// decomp: MAINMENU.PRG vs_mainMenu_unequipWeapon
// (src/MENU/MAINMENU.PRG/2D10.c:1008)
ItemsStatus UnequipWeapon(AshleyWeaponState& ashley,
                          InventoryContents& inventory, EquipStatusHost& host) {
  const auto status = EquipAshleyWeapon(ashley, nullptr, inventory, host);
  for (auto& weapon : inventory.weapons) weapon.is_equipped = 0;
  return status;
}

// decomp: MAINMENU.PRG vs_mainMenu_unequipShield
// (src/MENU/MAINMENU.PRG/2D10.c:1019)
ItemsStatus UnequipShield(ShieldLoadout& ashley_shield,
                          InventoryContents& inventory, EquipStatusHost& host) {
  const auto status =
      EquipAshleyShield(ashley_shield, nullptr, inventory, host);
  for (auto& shield : inventory.shields) shield.is_equipped = 0;
  return status;
}

// decomp: MAINMENU.PRG vs_mainMenu_initItem (src/MENU/MAINMENU.PRG/2D10.c:1030)
// The original cleared the record before the table when a weapon had no blade
// or grip, or a slot was outside its table.
ItemsStatus ClearItemSlot(ItemCategory category, std::int32_t slot,
                          InventoryContents& inventory) {
  switch (category) {
    case ItemCategory::kWeapon:
      return ClearWeaponSlot(slot, inventory);
    case ItemCategory::kBlade:
      return ResetSlot(inventory.blades, slot);
    case ItemCategory::kGrip:
      return ResetSlot(inventory.grips, slot);
    case ItemCategory::kShield:
      return ClearShieldSlot(slot, inventory);
    case ItemCategory::kArmor:
      return ResetSlot(inventory.armor, slot);
    case ItemCategory::kGem:
      return ResetSlot(inventory.gems, slot);
    case ItemCategory::kMisc:
      return ResetSlot(inventory.misc, slot);
    case ItemCategory::kManage:
      break;
  }
  return ItemsStatus::kOk;
}

// decomp: MAINMENU.PRG _getItemId (src/MENU/MAINMENU.PRG/2D10.c:1181)
std::optional<std::uint16_t> ItemOccupancyId(
    ItemCategory category, std::size_t index,
    const InventoryContents& inventory) {
  switch (category) {
    case ItemCategory::kWeapon:
      return OccupancyAt(
          inventory.weapons, index,
          [](const InventoryWeapon& item) { return item.blade; });
    case ItemCategory::kBlade:
      return OccupancyAt(inventory.blades, index,
                         [](const InventoryBlade& item) { return item.id; });
    case ItemCategory::kGrip:
      return OccupancyAt(inventory.grips, index,
                         [](const InventoryGrip& item) { return item.id; });
    case ItemCategory::kShield:
      return OccupancyAt(
          inventory.shields, index,
          [](const InventoryShield& item) { return item.base.id; });
    case ItemCategory::kArmor:
      return OccupancyAt(inventory.armor, index,
                         [](const InventoryArmor& item) { return item.id; });
    case ItemCategory::kGem:
      return OccupancyAt(inventory.gems, index,
                         [](const InventoryGem& item) { return item.id; });
    case ItemCategory::kMisc:
      return OccupancyAt(inventory.misc, index,
                         [](const InventoryMisc& item) { return item.id; });
    case ItemCategory::kManage:
      break;
  }
  return std::uint16_t{0};
}

std::span<std::uint8_t> DisplayOrder(InventoryIndices& indices,
                                     ItemCategory category) {
  switch (category) {
    case ItemCategory::kWeapon:
      return indices.weapons;
    case ItemCategory::kBlade:
      return indices.blades;
    case ItemCategory::kGrip:
      return indices.grips;
    case ItemCategory::kShield:
      return indices.shields;
    case ItemCategory::kArmor:
      return indices.armor;
    case ItemCategory::kGem:
      return indices.gems;
    case ItemCategory::kMisc:
      return indices.misc;
    case ItemCategory::kManage:
      break;
  }
  return {};
}

// decomp: MAINMENU.PRG vs_mainMenu_findItem (src/MENU/MAINMENU.PRG/2D10.c:1222)
std::int32_t FindDisplayPosition(std::span<const std::uint8_t> order,
                                 std::int32_t slot) {
  const auto listed = static_cast<std::int64_t>(slot) + 1;
  const auto entry = std::find(order.begin(), order.end(), listed);
  if (entry == order.end()) return 0;
  return static_cast<std::int32_t>(entry - order.begin()) + 1;
}

// decomp: MAINMENU.PRG _setItemIndex (src/MENU/MAINMENU.PRG/2D10.c:1241)
void AppendDisplayIndex(std::span<std::uint8_t> order, std::int32_t slot) {
  const auto free = std::find(order.begin(), order.end(), std::uint8_t{0});
  if (free != order.end()) {
    *free = static_cast<std::uint8_t>(static_cast<std::uint32_t>(slot) + 1U);
  }
}

// decomp: MAINMENU.PRG vs_mainMenu_rebuildInventory
// (src/MENU/MAINMENU.PRG/2D10.c:1254)
// The original read past the table for an entry naming a slot outside it;
// such entries are dropped and reported.
ItemsStatus RebuildDisplayIndices(ItemCategory category,
                                  InventoryIndices& indices,
                                  const InventoryContents& inventory) {
  const auto order = DisplayOrder(indices, category);
  if (order.empty()) return ItemsStatus::kCategoryOutOfRange;
  auto status = ItemsStatus::kOk;
  for (auto& entry : order) {
    if (entry == 0) continue;
    const auto id = ItemOccupancyId(category, entry - 1U, inventory);
    if (!id) KeepFirstRefusal(status, ItemsStatus::kDisplayEntryOutOfRange);
    if (id.value_or(0) == 0) entry = 0;
  }
  // The original closed each gap by shifting the rest of the order down.
  const auto listed_end = std::remove(order.begin(), order.end(), 0);
  std::fill(listed_end, order.end(), 0);
  for (std::size_t slot = 0; slot < order.size(); ++slot) {
    const auto signed_slot = static_cast<std::int32_t>(slot);
    if (ItemOccupancyId(category, slot, inventory).value_or(0) != 0 &&
        FindDisplayPosition(order, signed_slot) == 0) {
      AppendDisplayIndex(order, signed_slot);
    }
  }
  return status;
}

// decomp: MAINMENU.PRG vs_mainMenu_getItemCount
// (src/MENU/MAINMENU.PRG/2D10.c:1307)
std::int32_t CountItems(ItemCategory category,
                        const InventoryContents& inventory) {
  std::int32_t count = 0;
  for (std::size_t i = 0; i < kCategoryCapacities[CategoryIndex(category)];
       ++i) {
    if (ItemOccupancyId(category, i, inventory).value_or(0) != 0) ++count;
  }
  return count;
}

// decomp: MAINMENU.PRG vs_mainMenu_getFirstEmptyItemSlot
// (src/MENU/MAINMENU.PRG/2D10.c:1321)
std::size_t FirstEmptySlot(ItemCategory category,
                           const InventoryContents& inventory) {
  const std::size_t capacity = kCategoryCapacities[CategoryIndex(category)];
  for (std::size_t i = 0; i < capacity; ++i) {
    if (ItemOccupancyId(category, i, inventory).value_or(0) == 0) return i;
  }
  return capacity;
}

// decomp: MAINMENU.PRG vs_mainMenu_copyItem (src/MENU/MAINMENU.PRG/2D10.c:1334)
// The probe before a misc merge is a port fix: a misc claim that cannot fit
// whole leaves the inventory (and the loot row) unchanged.
ItemCopy CopyItem(std::int32_t flags, InventoryContents& target,
                  std::int32_t source_index, const InventoryContents& source,
                  InventoryIndices& displayed_indices,
                  const InventoryContents& displayed) {
  const auto category_bits =
      static_cast<std::size_t>(flags & kCopyItemCategoryMask);
  if (category_bits >= kItemCategoryCount) {
    return {0, ItemsStatus::kCategoryOutOfRange};
  }
  const auto category = static_cast<ItemCategory>(category_bits);
  const CopyRequest request{flags >> kCopyItemParentShift,
                            (flags & kCopyItemWrite) != 0, source_index,
                            CountItems(category, target),
                            FirstEmptySlot(category, target)};
  const PartTables tables{&target, &source, &displayed_indices, &displayed};
  const auto set_part_parent = [](auto& part, std::int32_t parent) {
    part.assembled_weapon_index =
        static_cast<decltype(part.assembled_weapon_index)>(parent);
  };
  switch (category) {
    case ItemCategory::kWeapon:
      return CopyWeapon(request, tables);
    case ItemCategory::kBlade:
      return CopyPart(request, target.blades, source.blades, set_part_parent);
    case ItemCategory::kGrip:
      return CopyPart(request, target.grips, source.grips, set_part_parent);
    case ItemCategory::kShield:
      return CopyShield(request, tables);
    case ItemCategory::kArmor:
      return CopyPart(request, target.armor, source.armor,
                      [](InventoryArmor&, std::int32_t) {});
    case ItemCategory::kGem:
      return CopyPart(request, target.gems, source.gems,
                      [](InventoryGem& gem, std::int32_t parent) {
                        gem.set_item_index = static_cast<std::uint16_t>(parent);
                      });
    case ItemCategory::kMisc:
      return CopyMisc(request, tables);
    case ItemCategory::kManage:
      break;
  }
  return {};
}

// decomp: MAINMENU.PRG vs_mainMenu_initInventory
// (src/MENU/MAINMENU.PRG/C48.c:44)
ItemsStatus StoreEquippedStats(EquippedLoadouts& ashley,
                               InventoryContents& inventory) {
  auto status = ItemsStatus::kOk;
  const auto equipped = [&status](auto& table, std::uint8_t reference) {
    auto* slot = ReferencedSlot(table, reference);
    if (slot == nullptr && reference != 0) {
      KeepFirstRefusal(status, ItemsStatus::kPartReferenceOutOfRange);
    }
    return slot;
  };
  if (auto* slot = equipped(inventory.weapons, ashley.weapon.index)) {
    KeepFirstRefusal(
        status, StoreInventoryWeaponStats(*slot, ashley.weapon, inventory));
  }
  if (auto* slot = equipped(inventory.shields, ashley.shield.index)) {
    KeepFirstRefusal(
        status, StoreInventoryShieldStats(*slot, ashley.shield, inventory));
  }
  for (auto& limb : ashley.limb_armor) {
    if (auto* slot = equipped(inventory.armor, limb.index)) {
      StoreInventoryArmorStats(*slot, limb);
    }
  }
  if (auto* slot = equipped(inventory.armor, ashley.accessory.index)) {
    StoreInventoryAccessoryStats(*slot, ashley.accessory);
  }
  return status;
}
}  // namespace VS::Items::Inventory
