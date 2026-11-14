#include "Menu/Inventory/InventorySorting.hpp"

#include <cstddef>
#include <type_traits>

#include "Items/EquipmentStats.hpp"
#include "Menu/Framework/ItemSorting.hpp"

namespace VS::Menu::Inventory {
namespace {
constexpr auto kOrderBounds = "sort row outside captured inventory order";

template <typename Table>
auto Read(const Table& table, std::int32_t index, SortingWorld& world) {
  if (index < 0 || static_cast<std::size_t>(index) >= table.size()) {
    world.Refuse("sort source index outside inventory table");
  }
  return table[static_cast<std::size_t>(index)];
}

void CheckStats(Items::ItemsStatus status, SortingWorld& world) {
  if (status != Items::ItemsStatus::kOk) world.Refuse(Items::Describe(status));
}

template <typename Record>
Record EquipmentRecord(const Items::InventoryContents& inventory,
                       std::int32_t index, SortingWorld& world) {
  if constexpr (std::is_same_v<Record, Items::InventoryBlade>) {
    return Read(inventory.blades, index, world);
  } else if constexpr (std::is_same_v<Record, Items::InventoryGrip>) {
    return Read(inventory.grips, index, world);
  } else if constexpr (std::is_same_v<Record, Items::InventoryArmor>) {
    return Read(inventory.armor, index, world);
  } else {
    static_assert(std::is_same_v<Record, Items::InventoryGem>);
    return Read(inventory.gems, index, world);
  }
}
}  // namespace

// decomp: _sortWeapons (MENU/items1.h:731); MENU3 80103CC8..80103E23.
void SortWeapons(SortingWorld& world, std::int32_t stat) {
  auto order = world.FixedOrder(Items::ItemCategory::kWeapon);
  Items::WeaponLoadout target{};
  Framework::sorting::StableOrder(
      world, order, Items::kWeaponSlots, kOrderBounds, [&](std::uint8_t token) {
        const auto inventory = world.CurrentInventory();
        const auto record = Read(inventory.weapons, token - 1, world);
        CheckStats(Items::LoadInventoryWeaponStats(target, &record, inventory),
                   world);
        return world.WeaponValue(stat, target);
      });
}

// decomp: _sortShields (MENU/items1.h:827); MENU3 80103F00..8010406B.
void SortShields(SortingWorld& world, std::int32_t stat) {
  auto order = world.FixedOrder(Items::ItemCategory::kShield);
  Items::ShieldLoadout target{};
  Framework::sorting::StableOrder(
      world, order, Items::kShieldSlots, kOrderBounds, [&](std::uint8_t token) {
        const auto inventory = world.CurrentInventory();
        const auto record = Read(inventory.shields, token - 1, world);
        CheckStats(Items::LoadInventoryShieldStats(target, &record, inventory),
                   world);
        return world.ShieldValue(stat, target);
      });
}

// decomp: _sortMisc (MENU/items1.h:899); MENU3 8010408C..801041CB.
void SortMisc(SortingWorld& world, std::int32_t stat) {
  auto order = world.FixedOrder(Items::ItemCategory::kMisc);
  Framework::sorting::StableOrder(
      world, order, Items::kMiscSlots, kOrderBounds, [&](std::uint8_t token) {
        const auto inventory = world.CurrentInventory();
        return world.MiscValue(stat, Read(inventory.misc, token - 1, world));
      });
}

bool HasEquipmentProjection(std::int32_t category) {
  return Framework::sorting::HasEquipmentProjection(category);
}

// decomp: _copyEquipment (MENU/items1.h:994); MENU3 801042C4..8010439B.
void CopyEquipmentStats(Items::EquipmentPart& target, std::int32_t category,
                        std::int32_t index, SortingWorld& world) {
  if (!HasEquipmentProjection(category)) return;
  const auto inventory = world.CurrentInventory();
  Framework::sorting::ProjectEquipmentStats(
      target, category, [&]<typename Record>() {
        return EquipmentRecord<Record>(inventory, index, world);
      });
}

// decomp: _sortEquipment (MENU/items1.h:1017); MENU3 8010439C..8010452F.
void SortEquipment(SortingWorld& world, std::int32_t category,
                   std::int32_t stat) {
  if (category < 0 ||
      static_cast<std::size_t>(category) >=
          static_cast<std::size_t>(Items::ItemCategory::kManage)) {
    world.Refuse("sort category outside inventory order table");
  }
  const auto selected = static_cast<Items::ItemCategory>(category);
  const auto count = world.Capacity(selected);
  auto order = world.EquipmentOrder(selected);
  Items::EquipmentPart target{};
  Framework::sorting::StableOrder(
      world, order, count, kOrderBounds, [&](std::uint8_t token) {
        if (!HasEquipmentProjection(category)) {
          world.Refuse("sort category has no equipment projection");
        }
        CopyEquipmentStats(target, category, token - 1, world);
        return world.EquipmentValue(stat, target);
      });
}
}  // namespace VS::Menu::Inventory
