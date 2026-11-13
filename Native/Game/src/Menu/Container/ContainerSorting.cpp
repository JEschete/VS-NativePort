#include "Menu/Container/ContainerSorting.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <variant>
#include <vector>

#include "Items/EquipmentStats.hpp"
#include "Menu/Container/ContainerDetailHelpers.hpp"

namespace VS::Menu::container {
namespace {
constexpr auto kWeapon = Items::ItemCategory::kWeapon;
constexpr auto kShield = Items::ItemCategory::kShield;
constexpr auto kMisc = Items::ItemCategory::kMisc;

constexpr std::size_t Extent(Items::ItemCategory category) {
  return Items::kContainerCapacities[static_cast<std::size_t>(category)];
}

std::uint16_t& Index(SortingWorld& world, std::span<std::uint16_t> order,
                     std::size_t row) {
  if (row >= order.size()) {
    world.Refuse("sort row outside captured container order");
  }
  return order[row];
}

template <typename Key>
void StableOrder(SortingWorld& world, std::span<std::uint16_t> order,
                 std::size_t count, Key key) {
  std::vector<std::uint16_t> sorted(count);
  std::size_t selected = 0;
  for (;;) {
    auto highest = std::numeric_limits<std::int32_t>::min();
    for (std::size_t row = 0; row < count; ++row) {
      const auto token = Index(world, order, row);
      if (token != 0) highest = std::max(highest, key(token));
    }
    if (highest == std::numeric_limits<std::int32_t>::min()) break;
    for (std::size_t row = 0; row < count; ++row) {
      const auto token = Index(world, order, row);
      if (token != 0 && key(token) == highest) {
        if (selected >= sorted.size()) {
          world.Refuse("sort selected more entries than its captured capacity");
        }
        sorted[selected++] = token;
        Index(world, order, row) = 0;
      }
    }
  }
  std::copy(sorted.begin(), sorted.end(), order.begin());
}

template <typename Record>
Record Read(Items::ContainerWorld& records, Items::ItemCategory category,
            std::uint16_t token) {
  const auto item = records.Read(category, static_cast<std::size_t>(token) - 1);
  const auto* typed = std::get_if<Record>(&item);
  if (typed == nullptr) {
    records.Refuse("sort record type disagrees with selected category");
  }
  return *typed;
}

template <typename Record, typename Reader>
void Project(Items::EquipmentPart& target, const Items::ContainerRecord* source,
             Items::ContainerWorld& records, Reader reader) {
  if (source == nullptr) records.Refuse("missing equipment projection source");
  const auto* typed = std::get_if<Record>(source);
  if (typed == nullptr) {
    records.Refuse("equipment projection source disagrees with category");
  }
  reader(target, *typed);
}
}  // namespace

// decomp: _sortWeaponsByStat (MENU/MENUD.PRG/234.c:1306-1356);
// original instructions 801051F8..80105377.
void SortWeapons(SortingWorld& world, std::int32_t stat) {
  auto& source = world.SourceRecords();
  auto order = world.Order(kWeapon);
  Items::WeaponLoadout target{};
  StableOrder(world, order, Extent(kWeapon), [&](std::uint16_t token) {
    const auto record = Read<Items::InventoryWeapon>(source, kWeapon, token);
    static_cast<void>(
        BuildWeaponStats(target, &record, world.CurrentRecords()));
    return world.WeaponValue(stat, target);
  });
}

// decomp: _sortShieldsByStat (MENU/MENUD.PRG/234.c:1388-1435);
// original instructions 80105454..801055CF.
void SortShields(SortingWorld& world, std::int32_t stat) {
  auto& source = world.SourceRecords();
  auto order = world.Order(kShield);
  Items::ShieldLoadout target{};
  StableOrder(world, order, Extent(kShield), [&](std::uint16_t token) {
    const auto record = Read<Items::InventoryShield>(source, kShield, token);
    BuildShieldStats(target, &record, world.CurrentRecords());
    return world.ShieldValue(stat, target);
  });
}

// decomp: _sortItemsByStat (MENU/MENUD.PRG/234.c:1445-1487);
// original instructions 801055F0..8010574B.
void SortMisc(SortingWorld& world, std::int32_t stat) {
  auto& source = world.SourceRecords();
  auto order = world.Order(kMisc);
  StableOrder(world, order, Extent(kMisc), [&](std::uint16_t token) {
    return world.MiscValue(stat,
                           Read<Items::InventoryMisc>(source, kMisc, token));
  });
}

bool HasEquipmentProjection(std::int32_t category) {
  return category == static_cast<std::int32_t>(Items::ItemCategory::kBlade) ||
         category == static_cast<std::int32_t>(Items::ItemCategory::kGrip) ||
         category == static_cast<std::int32_t>(Items::ItemCategory::kArmor) ||
         category == static_cast<std::int32_t>(Items::ItemCategory::kGem);
}

// decomp: _sortEquipment (MENU/MENUD.PRG/234.c:1528-1547);
// original instructions 80105844..8010592B. Both actual callers supply
// F02968's independent stack output, not byte-overlapping container storage.
void CopyEquipmentStats(Items::EquipmentPart& target, std::int32_t category,
                        const Items::ContainerRecord* source,
                        Items::ContainerWorld& records) {
  if (!HasEquipmentProjection(category)) return;
  switch (static_cast<Items::ItemCategory>(category)) {
    case Items::ItemCategory::kBlade:
      Project<Items::InventoryBlade>(target, source, records,
                                     Items::ReadInventoryBladeStats);
      return;
    case Items::ItemCategory::kGrip:
      Project<Items::InventoryGrip>(target, source, records,
                                    Items::ReadInventoryGripStats);
      return;
    case Items::ItemCategory::kArmor:
      Project<Items::InventoryArmor>(target, source, records,
                                     Items::ReadInventoryArmorStats);
      return;
    case Items::ItemCategory::kGem:
      Project<Items::InventoryGem>(target, source, records,
                                   Items::ReadInventoryGemStats);
      return;
    default:
      return;
  }
}

// decomp: _copyEquipmentByStat (MENU/MENUD.PRG/234.c:1549-1599);
// original instructions 8010592C..80105ACB.
void SortEquipment(SortingWorld& world, std::int32_t category,
                   std::int32_t stat) {
  if (category < 0 ||
      static_cast<std::size_t>(category) >= Items::kItemCategoryCount) {
    world.Refuse("sort category outside container categories");
  }
  const auto selected = static_cast<Items::ItemCategory>(category);
  const auto count = world.Capacity(selected);
  auto order = world.Order(selected);
  Items::EquipmentPart target{};
  StableOrder(world, order, count, [&](std::uint16_t token) {
    if (!HasEquipmentProjection(category)) {
      world.Refuse("sort category has no equipment projection");
    }
    auto& records = world.CurrentRecords();
    const auto record =
        records.Read(selected, static_cast<std::size_t>(token) - 1);
    CopyEquipmentStats(target, category, &record, records);
    return world.EquipmentValue(stat, target);
  });
}
}  // namespace VS::Menu::container
