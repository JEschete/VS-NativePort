#include "Menu/Loot/LootStaging.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Menu::Loot {
namespace {
using VS::Items::InventoryContents;
using VS::Items::InventoryIndices;
using VS::Items::ItemCategory;
namespace inventory_api = VS::Items::Inventory;

template <typename Record, std::size_t N>
int StagePart(ItemCategory category,
              std::array<Record, N> InventoryContents::* table,
              const Record& item, int parent, InventoryContents& target) {
  InventoryContents source{};
  (source.*table)[0] = item;
  InventoryIndices indices{};
  const auto parent_bits = static_cast<std::uint32_t>(parent)
                           << inventory_api::kCopyItemParentShift;
  const auto flags = static_cast<std::int32_t>(
      parent_bits | static_cast<std::uint32_t>(category) |
      static_cast<std::uint32_t>(inventory_api::kCopyItemWrite));
  const auto copied =
      inventory_api::CopyItem(flags, target, 0, source, indices, source);
  return copied.status == VS::Items::ItemsStatus::kOk ? copied.slot : 0;
}

int CopyItem(ItemCategory category, InventoryContents& target,
             const InventoryContents& source) {
  InventoryIndices indices{};
  const auto flags =
      static_cast<std::int32_t>(category) | inventory_api::kCopyItemWrite;
  const auto copied =
      inventory_api::CopyItem(flags, target, 0, source, indices, source);
  return copied.status == VS::Items::ItemsStatus::kOk ? copied.slot : 0;
}
}  // namespace

// decomp: MENU/MENUB.PRG _copyBladeToInventory (260.c:712-730);
// MIPS 0x80107c54-0x80107ce7.
int StageBlade(const Items::InventoryBlade& blade, int weapon_index,
               Items::InventoryContents& inventory) {
  return StagePart(ItemCategory::kBlade, &InventoryContents::blades, blade,
                   weapon_index, inventory);
}

// decomp: MENU/MENUB.PRG _copyGripToInventory (260.c:739-757);
// MIPS 0x80107ce8-0x80107d7b.
int StageGrip(const Items::InventoryGrip& grip, int weapon_index,
              Items::InventoryContents& inventory) {
  return StagePart(ItemCategory::kGrip, &InventoryContents::grips, grip,
                   weapon_index, inventory);
}

// decomp: MENU/MENUB.PRG _copyGemToInventory (260.c:766-784);
// MIPS 0x80107d7c-0x80107e0f.
int StageGem(const Items::InventoryGem& gem, int item_index,
             Items::InventoryContents& inventory) {
  return StagePart(ItemCategory::kGem, &InventoryContents::gems, gem,
                   item_index, inventory);
}

// decomp: MENU/MENUB.PRG _copyWeaponToInventory (260.c:792-820);
// MIPS 0x80107e10-0x80107f1b.
int StageWeapon(const Items::LootedWeapon& looted_weapon,
                Items::InventoryContents& inventory) {
  InventoryContents source{};
  source.blades[0] = looted_weapon.blade;
  source.grips[0] = looted_weapon.grip;
  auto& weapon = source.weapons[0];
  weapon.blade = 1;
  weapon.grip = 1;
  weapon.name = looted_weapon.name;
  for (std::size_t i = 0; i < Items::kWeaponGemCount; ++i) {
    if (looted_weapon.gems[i].id == 0) continue;
    source.gems[i] = looted_weapon.gems[i];
    weapon.gems[i] = static_cast<std::uint8_t>(i + 1);
  }
  return CopyItem(ItemCategory::kWeapon, inventory, source);
}

// decomp: MENU/MENUB.PRG _copyArmorToInventory (260.c:827-844);
// MIPS 0x80107f1c-0x80107f9b.
int StageArmor(const Items::InventoryArmor& armor,
               Items::InventoryContents& inventory) {
  return StagePart(ItemCategory::kArmor, &InventoryContents::armor, armor, 0,
                   inventory);
}

// decomp: MENU/MENUB.PRG _copyShieldToInventory (260.c:852-879);
// MIPS 0x80107f9c-0x8010808b.
int StageShield(const Items::LootedShield& looted_shield,
                Items::InventoryContents& inventory) {
  const auto gem_count = static_cast<std::int32_t>(
      std::count_if(looted_shield.gems.begin(), looted_shield.gems.end(),
                    [](const auto& gem) { return gem.id != 0; }));
  if (inventory_api::CountItems(ItemCategory::kShield, inventory) ==
          static_cast<std::int32_t>(Items::kShieldSlots) ||
      inventory_api::CountItems(ItemCategory::kGem, inventory) + gem_count >
          static_cast<std::int32_t>(Items::kGemSlots)) {
    return 0;
  }

  auto staged = inventory;
  const auto slot =
      inventory_api::FirstEmptySlot(ItemCategory::kShield, staged);
  if (slot >= Items::kShieldSlots) return 0;
  const auto index = static_cast<int>(slot + 1);
  auto& shield = staged.shields[slot];
  shield = {};
  shield.base = looted_shield.shield;
  shield.base.limb = static_cast<std::uint8_t>(index);

  for (std::size_t i = 0; i < Items::kWeaponGemCount; ++i) {
    if (looted_shield.gems[i].id == 0) continue;
    const auto gem_index =
        StagePart(ItemCategory::kGem, &InventoryContents::gems,
                  looted_shield.gems[i], index | Items::kShieldGemFlag, staged);
    if (gem_index == 0) return 0;
    shield.gems[i] = static_cast<std::uint8_t>(gem_index);
  }
  inventory = staged;
  return index;
}

// decomp: MENU/MENUB.PRG _copyMiscToInventory (260.c:886-902);
// MIPS 0x8010808c-0x801080ef.
int StageMiscItem(const Items::LootedItem& looted_item,
                  Items::InventoryContents& inventory) {
  const auto slot =
      inventory_api::FirstEmptySlot(ItemCategory::kMisc, inventory);
  if (slot >= Items::kMiscSlots) return 0;
  auto& item = inventory.misc[slot];
  item.id = looted_item.item_id;
  item.count = looted_item.count;
  return static_cast<int>(slot + 1);
}
}  // namespace VS::Menu::Loot
