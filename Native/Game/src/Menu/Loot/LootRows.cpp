// Hand-maintained (tools/port/owned.csv).
#include "Menu/Loot/LootRows.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

#include "Core/Math/ScalarMath.hpp"
#include "Items/ItemRecords.hpp"
#include "Menu/Loot/LootStaging.hpp"

namespace VS::Menu::Loot {
namespace {
using VS::Items::BattleLoot;
using VS::Items::InventoryContents;
using VS::Items::ItemCategory;
using VS::Items::ItemsStatus;

constexpr std::uint8_t kLastVisibleRow = kVisibleLootRows - 1;

// decomp: _updateLootList's tail (src/MENU/MENUB.PRG/260.c:921-942): keeps the
// selection in range and, past the first page, one row below it visible.
void ClampCursor(LootCursor& cursor) {
  const std::int32_t count = cursor.count;
  const std::int32_t last_row = count - 1;
  std::int32_t position = cursor.selected + cursor.offset;
  if (position >= count) {
    position = last_row;
    cursor.offset = static_cast<std::uint8_t>(count - kVisibleLootRows);
    cursor.selected = kLastVisibleRow;
  }
  if (count <= kVisibleLootRows) {
    cursor.selected = static_cast<std::uint8_t>(position);
    cursor.offset = 0;
    return;
  }
  const std::int32_t last_offset = count - kVisibleLootRows;
  if (last_offset < cursor.offset) {
    cursor.offset = static_cast<std::uint8_t>(last_offset);
    cursor.selected = static_cast<std::uint8_t>(position - last_offset);
  }
  if (cursor.selected == kLastVisibleRow && position != last_row) {
    cursor.selected = kLastVisibleRow - 1;
    cursor.offset = static_cast<std::uint8_t>(cursor.offset + 1);
  }
}

// The set gems among a weapon's or shield's first `gem_slots` sockets.
CategoryCount CountSetGems(std::span<const std::uint8_t> gems,
                           std::uint8_t gem_slots) {
  const auto sockets =
      gems.first(std::min<std::size_t>(gem_slots, gems.size()));
  return {
      .count = static_cast<std::int32_t>(std::ranges::count_if(
          sockets, [](std::uint8_t gem) { return gem != 0; })),
      .status = gem_slots > gems.size() ? ItemsStatus::kSlotOutOfRange
                                        : ItemsStatus::kOk,
  };
}

CategoryCount CountWeaponGems(const LootRow& row,
                              const InventoryContents& inventory) {
  if (row.item_index >= inventory.weapons.size()) {
    return {.status = ItemsStatus::kSlotOutOfRange};
  }
  const auto& weapon = inventory.weapons[row.item_index];
  if (weapon.grip == 0 || weapon.grip > inventory.grips.size()) {
    return {.status = ItemsStatus::kPartReferenceOutOfRange};
  }
  return CountSetGems(weapon.gems, inventory.grips[weapon.grip - 1].gem_slots);
}

CategoryCount CountShieldGems(const LootRow& row,
                              const InventoryContents& inventory) {
  if (row.item_index >= inventory.shields.size()) {
    return {.status = ItemsStatus::kSlotOutOfRange};
  }
  const auto& shield = inventory.shields[row.item_index];
  return CountSetGems(shield.gems, shield.base.gem_slots);
}

std::int32_t Matches(std::uint8_t category, ItemCategory counted) {
  return category == static_cast<std::uint8_t>(counted) ? 1 : 0;
}

DropSlot NthSlot(DropSlot first, std::size_t index) {
  return static_cast<DropSlot>(static_cast<std::size_t>(first) + index);
}

void ClearDrop(BattleLoot& record, DropSlot slot) {
  const auto index = static_cast<std::size_t>(slot);
  switch (slot) {
    case DropSlot::kWeapon:
      record.weapon.header.difficulty_flags = 0;
      return;
    case DropSlot::kBlade:
      record.blade.header.difficulty_flags = 0;
      return;
    case DropSlot::kGrip:
      record.grip.header.difficulty_flags = 0;
      return;
    case DropSlot::kShield:
      record.shield.header.difficulty_flags = 0;
      return;
    case DropSlot::kArmor0:
    case DropSlot::kArmor1:
      record.armor[index - static_cast<std::size_t>(DropSlot::kArmor0)]
          .header.difficulty_flags = 0;
      return;
    case DropSlot::kAccessory:
      record.accessory.header.difficulty_flags = 0;
      return;
    case DropSlot::kGem:
      record.gem.header.difficulty_flags = 0;
      return;
    case DropSlot::kMisc0:
    case DropSlot::kMisc1:
    case DropSlot::kMisc2:
      record.misc[index - static_cast<std::size_t>(DropSlot::kMisc0)]
          .difficulty_flags = 0;
      return;
  }
}

// Stages one eligible drop. A drop with no free row, or that the bag cannot
// hold (the original added row index 0xFF), waits for the next batch.
template <typename Stage>
void StageDrop(LootStaging& staging, StagedDrop drop, ItemCategory category,
               Stage&& stage) {
  auto& batch = staging.batch;
  if (batch.count >= batch.drops.size() ||
      staging.cursor.count >= staging.rows.size()) {
    batch.deferred = true;
    return;
  }
  const std::int32_t slot = std::forward<Stage>(stage)(staging.inventory);
  if (slot == 0 || !AppendLootRow(staging, category, slot)) {
    batch.deferred = true;
    return;
  }
  batch.drops[batch.count] = drop;
  ++batch.count;
}
}  // namespace

// decomp: MENU/MENUB.PRG updateLootList (src/MENU/MENUB.PRG/260.c:910-946; was
// _updateLootList); MIPS 0x801080f0-0x8010822b.
void RefreshClaimableRows(LootStaging& staging, ClaimCheck& check) {
  const auto rows =
      std::min<std::size_t>(staging.cursor.count, staging.rows.size());
  for (auto& row : std::span(staging.rows).first(rows)) {
    row.state = check.Fits(row, staging.inventory) ? 1 : 0;
  }
  ClampCursor(staging.cursor);
}

// decomp: MENU/MENUB.PRG addLootToList (src/MENU/MENUB.PRG/260.c:953-959; was
// _addLootToList); MIPS 0x8010822c-0x80108263.
bool AppendLootRow(LootStaging& staging, ItemCategory category,
                   std::int32_t slot) {
  auto& cursor = staging.cursor;
  if (cursor.count >= staging.rows.size()) return false;
  auto& row = staging.rows[cursor.count];
  row.category = static_cast<std::uint8_t>(category);
  row.item_index =
      static_cast<std::uint8_t>(Core::Math::WrappingSubtract(slot, 1));
  row.slide_step = 0;
  cursor.count = static_cast<std::uint8_t>(cursor.count + 1);
  return true;
}

// decomp: MENU/MENUB.PRG itemIncludesItemsInCategory
// (src/MENU/MENUB.PRG/260.c:971-1013; was _itemIncludesItemsInCategory); MIPS
// 0x80108264-0x8010837b.
CategoryCount CountContainedCategory(ItemCategory counted, const LootRow& row,
                                     const InventoryContents& inventory) {
  switch (counted) {
    case ItemCategory::kBlade:
    case ItemCategory::kGrip:
      // A weapon carries one blade and one grip.
      return {.count = Matches(row.category, ItemCategory::kWeapon) +
                       Matches(row.category, counted)};
    case ItemCategory::kWeapon:
    case ItemCategory::kShield:
    case ItemCategory::kArmor:
    case ItemCategory::kMisc:
      return {.count = Matches(row.category, counted)};
    case ItemCategory::kGem:
      break;
    case ItemCategory::kManage:
      return {};
  }
  auto gems = Matches(row.category, ItemCategory::kWeapon) != 0
                  ? CountWeaponGems(row, inventory)
              : Matches(row.category, ItemCategory::kShield) != 0
                  ? CountShieldGems(row, inventory)
                  : CategoryCount{};
  gems.count += Matches(row.category, ItemCategory::kGem);
  return gems;
}

// decomp: MENU/MENUB.PRG droppableUnderCurrentDifficulty
// (src/MENU/MENUB.PRG/260.c:1116-1119; was _droppableUnderCurrentDifficulty);
// MIPS 0x801086dc-0x801086f3.
std::int32_t DropDifficultyMatch(std::int32_t difficulty_flags,
                                 std::uint8_t difficulty) {
  return difficulty_flags & (difficulty + 1);
}

// decomp: MENU/MENUB.PRG applyLootList (src/MENU/MENUB.PRG/260.c:1127-1170; was
// _applyLootList); MIPS 0x801086f4-0x801088d3. Native: a drop that gets no row
// or bag room stays in `record` for the next batch (see StageDrop); the
// original added it as row index 0xFF.
void StageDropRecord(const BattleLoot& record, std::size_t record_index,
                     LootStaging& staging, ClaimCheck& check) {
  const auto stage = [&staging, record_index](
                         std::uint8_t difficulty_flags, DropSlot slot,
                         ItemCategory category, auto&& stager) {
    if (DropDifficultyMatch(difficulty_flags, staging.difficulty) != 0) {
      StageDrop(staging, {.record = record_index, .slot = slot}, category,
                stager);
    }
  };
  stage(record.weapon.header.difficulty_flags, DropSlot::kWeapon,
        ItemCategory::kWeapon, [&record](InventoryContents& inventory) {
          return StageWeapon(record.weapon, inventory);
        });
  stage(record.blade.header.difficulty_flags, DropSlot::kBlade,
        ItemCategory::kBlade, [&record](InventoryContents& inventory) {
          return StageBlade(record.blade.blade, 0, inventory);
        });
  stage(record.grip.header.difficulty_flags, DropSlot::kGrip,
        ItemCategory::kGrip, [&record](InventoryContents& inventory) {
          return StageGrip(record.grip.grip, 0, inventory);
        });
  stage(record.shield.header.difficulty_flags, DropSlot::kShield,
        ItemCategory::kShield, [&record](InventoryContents& inventory) {
          return StageShield(record.shield, inventory);
        });
  for (std::size_t i = 0; i < record.armor.size(); ++i) {
    const auto& armor = record.armor[i];
    stage(armor.header.difficulty_flags, NthSlot(DropSlot::kArmor0, i),
          ItemCategory::kArmor, [&armor](InventoryContents& inventory) {
            return StageArmor(armor.armor, inventory);
          });
  }
  stage(record.accessory.header.difficulty_flags, DropSlot::kAccessory,
        ItemCategory::kArmor, [&record](InventoryContents& inventory) {
          return StageArmor(record.accessory.armor, inventory);
        });
  stage(record.gem.header.difficulty_flags, DropSlot::kGem, ItemCategory::kGem,
        [&record](InventoryContents& inventory) {
          return StageGem(record.gem.gem, 0, inventory);
        });
  for (std::size_t i = 0; i < record.misc.size(); ++i) {
    const auto& item = record.misc[i];
    if (item.item_id == 0 || item.count == 0) continue;
    stage(item.difficulty_flags, NthSlot(DropSlot::kMisc0, i),
          ItemCategory::kMisc, [&item](InventoryContents& inventory) {
            return StageMiscItem(item, inventory);
          });
  }
  RefreshClaimableRows(staging, check);
}

// decomp: MENU/MENUB.PRG applyAllLootLists (src/MENU/MENUB.PRG/260.c:1176-1188;
// was _applyAllLootLists); MIPS 0x801088d4-0x80108937. Native: the batch and
// cursor restart here; the original's caller cleared the count and offset.
void BuildStagedInventory(std::span<const BattleLoot> records,
                          LootStaging& staging, ClaimCheck& check) {
  staging.inventory = {};
  staging.cursor = {};
  staging.batch = {};
  for (std::size_t i = 0; i < records.size(); ++i) {
    StageDropRecord(records[i], i, staging, check);
  }
}

// Native: pages loot that did not fit one batch; called once the current
// batch is claimed or left behind.
bool AdvanceStagedInventory(std::span<BattleLoot> records, LootStaging& staging,
                            ClaimCheck& check) {
  auto& batch = staging.batch;
  if (!batch.deferred) return false;
  const auto staged = std::min(batch.count, batch.drops.size());
  for (const auto& drop : std::span(batch.drops).first(staged)) {
    if (drop.record < records.size()) {
      ClearDrop(records[drop.record], drop.slot);
    }
  }
  BuildStagedInventory(records, staging, check);
  staging.cursor.offset = 0;
  staging.cursor.selected = 0;
  return staging.cursor.count != 0;
}

// decomp: MENU/MENUB.PRG setKeyFlag (src/MENU/MENUB.PRG/260.c:2111-2116; was
// _setKeyFlag); MIPS 0x8010a24c-0x8010a27f.
std::optional<std::int16_t> ClaimedKeyFlag(std::int32_t item_id) {
  if (item_id < kFirstKeyItemId) return std::nullopt;
  return Core::Math::WrapSigned16(static_cast<std::uint32_t>(
      Core::Math::WrappingAdd(item_id, kKeyItemFlagBase)));
}
}  // namespace VS::Menu::Loot
