#include "Menu/Inventory/ItemListRows.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"

namespace VS::Menu::Inventory {
namespace {
using Items::ItemCategory;

constexpr auto Category(ItemCategory category) {
  return static_cast<std::int32_t>(category);
}

template <typename Table>
const typename Table::value_type* Slot(const Table& table, std::int32_t index) {
  if (index < 0 || static_cast<std::size_t>(index) >= table.size()) {
    return nullptr;
  }
  return &table[static_cast<std::size_t>(index)];
}

// The table a category's order names, or nullopt for the manage row.
std::optional<std::size_t> SlotCount(ItemCategory category) {
  if (category == ItemCategory::kManage) return std::nullopt;
  return Items::kCategoryCapacities[static_cast<std::size_t>(category)];
}

// A u8 capacity bounds every list.
constexpr std::size_t kMaxListRows = 256;

struct PlannedRow {
  std::size_t slot = 0;
  std::uint16_t parent_weapon = 0;
  std::uint8_t parent_shield = 0;
  std::uint8_t action = 0;
};

std::uint32_t OwnFlags(ItemCategory category, std::size_t slot,
                       const Items::InventoryContents& inventory) {
  bool equipped = false;
  switch (category) {
    case ItemCategory::kWeapon:
      equipped = inventory.weapons[slot].is_equipped != 0;
      break;
    case ItemCategory::kShield:
      equipped = inventory.shields[slot].is_equipped != 0;
      break;
    case ItemCategory::kArmor:
      equipped = inventory.armor[slot].limb != 0;
      break;
    default:
      break;
  }
  return equipped ? kEquippedRowFlags : 0;
}

std::uint32_t ParentFlags(bool parent_equipped) {
  return parent_equipped ? kEquippedRowFlags : kAssembledRowFlags;
}

// Checks one order entry of a list; `records_actions` maps misc actions.
std::expected<PlannedRow, ItemRowsRefusal> PlanRow(
    const ItemRowsInput& input, const Items::InventoryContents& inventory,
    std::size_t i, bool records_actions) {
  PlannedRow row{};
  row.slot = static_cast<std::size_t>(input.order[i] - 1);
  const auto slots = SlotCount(input.category);
  if (slots && row.slot >= *slots) {
    return std::unexpected(ItemRowsRefusal::kSlot);
  }
  if (records_actions) {
    if (i >= kMiscActionRows) {
      return std::unexpected(ItemRowsRefusal::kMiscRows);
    }
    // The map index is the 32-bit difference; ids below 323 wrap far out.
    const auto map_index =
        static_cast<std::uint32_t>(Core::Math::WrappingSubtract(
            inventory.misc[row.slot].id, kFirstMiscActionItem));
    if (map_index >= input.misc_action_map.size()) {
      return std::unexpected(ItemRowsRefusal::kMiscItem);
    }
    row.action = input.misc_action_map[map_index];
  }
  const auto category_id = Category(input.category);
  const auto slot_id = static_cast<std::int32_t>(row.slot);
  const auto weapon = ParentWeaponIndex(category_id, slot_id, inventory);
  const auto shield = ParentShieldIndex(category_id, slot_id, inventory);
  if (!weapon || !shield) return std::unexpected(ItemRowsRefusal::kSlot);
  if (*weapon > Items::kWeaponSlots) {
    return std::unexpected(ItemRowsRefusal::kParentWeapon);
  }
  if (*shield > Items::kShieldSlots) {
    return std::unexpected(ItemRowsRefusal::kParentShield);
  }
  row.parent_weapon = *weapon;
  row.parent_shield = *shield;
  return row;
}

// Writes one planned row: its format, misc action and flags.
void ApplyRow(const ItemRowsInput& input,
              const Items::InventoryContents& inventory, const PlannedRow& row,
              std::size_t i, bool records_actions, ItemRowsWorld& world) {
  std::uint32_t flags = 0;
  if (SlotCount(input.category)) {
    world.FormatRow(input.category, row.slot, i);
    flags = OwnFlags(input.category, row.slot, inventory);
  }
  if (records_actions) {
    auto action = static_cast<std::uint32_t>(row.action);
    if (action - kFirstUpgradableAction < kUpgradableActionCount) {
      for (std::int32_t step = 0; step < kActionUpgradeSteps; ++step) {
        if (world.ActionUnlocked(action)) ++action;
      }
    }
    world.StoreMiscAction(i, static_cast<std::uint8_t>(action));
  }
  if (row.parent_weapon != 0) {
    flags =
        ParentFlags(inventory.weapons[row.parent_weapon - 1U].is_equipped != 0);
  }
  if (row.parent_shield != 0) {
    flags =
        ParentFlags(inventory.shields[row.parent_shield - 1U].is_equipped != 0);
  }
  world.AddRowFlags(i, flags);
}
}  // namespace

// decomp: MENU3.PRG getParentWeaponIndex (src/MENU/items2.h:16; was
// _getParentWeaponIndex).
std::optional<std::uint16_t> ParentWeaponIndex(
    std::int32_t category, std::int32_t index,
    const Items::InventoryContents& inventory) {
  if (category == Category(ItemCategory::kBlade)) {
    const auto* blade = Slot(inventory.blades, index);
    if (blade == nullptr) return std::nullopt;
    return blade->assembled_weapon_index;
  }
  if (category == Category(ItemCategory::kGrip)) {
    const auto* grip = Slot(inventory.grips, index);
    if (grip == nullptr) return std::nullopt;
    return grip->assembled_weapon_index;
  }
  if (category == Category(ItemCategory::kGem)) {
    const auto* gem = Slot(inventory.gems, index);
    if (gem == nullptr) return std::nullopt;
    // The whole halfword is the weapon unless the shield flag is set.
    if ((gem->set_item_index & Items::kShieldGemFlag) != 0) return 0;
    return gem->set_item_index;
  }
  return 0;
}

// decomp: MENU3.PRG getParentShieldIndex (src/MENU/items2.h:43; was
// _getParentShieldIndex).
std::optional<std::uint8_t> ParentShieldIndex(
    std::int32_t category, std::int32_t index,
    const Items::InventoryContents& inventory) {
  if (category != Category(ItemCategory::kGem)) return 0;
  const auto* gem = Slot(inventory.gems, index);
  if (gem == nullptr) return std::nullopt;
  if ((gem->set_item_index & Items::kShieldGemFlag) == 0) return 0;
  return static_cast<std::uint8_t>(gem->set_item_index &
                                   Items::kShieldGemSlotMask);
}

// decomp: MENU3.PRG populateItemRows (src/MENU/items2.h:65; was
// _populateItemRows). The order and the capacity bound the list; every row is
// checked before the first is written.
std::expected<std::int32_t, ItemRowsRefusal> BuildItemRows(
    const ItemRowsInput& input, const Items::InventoryContents& inventory,
    ItemRowsWorld& world) {
  const bool records_actions = input.category == ItemCategory::kMisc &&
                               input.context == ItemRowsContext::kItemMenu;
  std::array<PlannedRow, kMaxListRows> planned{};
  std::size_t count = 0;
  for (std::size_t i = 0; i < input.capacity; ++i) {
    if (i >= input.order.size()) {
      return std::unexpected(ItemRowsRefusal::kOrder);
    }
    if (input.order[i] == 0) break;
    if (i >= input.row_capacity || i >= planned.size()) {
      return std::unexpected(ItemRowsRefusal::kRows);
    }
    const auto row = PlanRow(input, inventory, i, records_actions);
    if (!row) return std::unexpected(row.error());
    planned[i] = *row;
    count = i + 1;
  }
  for (std::size_t i = 0; i < count; ++i) {
    ApplyRow(input, inventory, planned[i], i, records_actions, world);
  }
  return static_cast<std::int32_t>(count);
}

// decomp: MENU3.PRG restoreItemMenu (src/MENU/items2.h:180; was
// _restoreItemMenu). Each row's cursor read follows its state update.
std::expected<void, ItemRowsRefusal> RestoreItemRows(
    ItemRowsRestoreWorld& world, std::int32_t category) {
  const auto memory_index = Core::Math::WrappingMultiply(
      Core::Math::WrappingAdd(category, kItemListCursorBase), 2);
  const auto in_memory = [&] {
    return memory_index >= 0 &&
           static_cast<std::size_t>(memory_index) < world.CursorMemory().size();
  };
  if (!in_memory()) return std::unexpected(ItemRowsRefusal::kCursorMemory);
  for (auto i = kFirstItemListRow; i < Framework::kNoRow; ++i) {
    if (world.Row(i) == nullptr) return std::unexpected(ItemRowsRefusal::kRow);
  }

  for (auto row = world.ListAnimationRow(); row < kItemListAnimationComplete;) {
    world.AdvanceListRows();
    const auto next = world.ListAnimationRow();
    if (next <= row) return std::unexpected(ItemRowsRefusal::kStalled);
    row = next;
  }

  for (auto i = kFirstItemListRow; i < Framework::kNoRow; ++i) {
    auto* row = world.Row(i);
    if (row == nullptr) return std::unexpected(ItemRowsRefusal::kRow);
    if (row->state == Framework::RowState::kSlideX) {
      row->state = Framework::RowState::kIdle;
      row->x = row->target0;
    }
    if (!in_memory()) return std::unexpected(ItemRowsRefusal::kCursorMemory);
    const auto remembered =
        world.CursorMemory()[static_cast<std::size_t>(memory_index)];
    row->selected = i == remembered + kFirstItemListRow ? 1 : 0;
  }
  return {};
}
}  // namespace VS::Menu::Inventory
