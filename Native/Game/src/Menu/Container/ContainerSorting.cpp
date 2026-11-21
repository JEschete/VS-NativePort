#include "Menu/Container/ContainerSorting.hpp"

#include <cstddef>
#include <variant>

#include "Menu/Container/ContainerDetailHelpers.hpp"
#include "Menu/Framework/ItemSorting.hpp"

namespace VS::Menu::container {
namespace {
constexpr auto kWeapon = Items::ItemCategory::kWeapon;
constexpr auto kShield = Items::ItemCategory::kShield;
constexpr auto kMisc = Items::ItemCategory::kMisc;
constexpr auto kOrderBounds = "sort row outside captured container order";

constexpr std::size_t Extent(Items::ItemCategory category) {
  return Items::kContainerCapacities[static_cast<std::size_t>(category)];
}

using Framework::sorting::StableOrder;

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

}  // namespace

// decomp: sortWeaponsByStat (src/MENU/MENUD.PRG/234.c:1307-1357; was
// _sortWeaponsByStat); original instructions 801051F8..80105377.
void SortWeapons(SortingWorld& world, std::int32_t stat) {
  auto& source = world.SourceRecords();
  auto order = world.Order(kWeapon);
  Items::WeaponLoadout target{};
  StableOrder(world, order, Extent(kWeapon), kOrderBounds,
              [&](std::uint16_t token) {
                const auto record =
                    Read<Items::InventoryWeapon>(source, kWeapon, token);
                static_cast<void>(
                    BuildWeaponStats(target, &record, world.CurrentRecords()));
                return world.WeaponValue(stat, target);
              });
}

// decomp: sortShieldsByStat (src/MENU/MENUD.PRG/234.c:1389-1436; was
// _sortShieldsByStat); original instructions 80105454..801055CF.
void SortShields(SortingWorld& world, std::int32_t stat) {
  auto& source = world.SourceRecords();
  auto order = world.Order(kShield);
  Items::ShieldLoadout target{};
  StableOrder(world, order, Extent(kShield), kOrderBounds,
              [&](std::uint16_t token) {
                const auto record =
                    Read<Items::InventoryShield>(source, kShield, token);
                BuildShieldStats(target, &record, world.CurrentRecords());
                return world.ShieldValue(stat, target);
              });
}

// decomp: sortItemsByStat (src/MENU/MENUD.PRG/234.c:1446-1488; was
// _sortItemsByStat); original instructions 801055F0..8010574B.
void SortMisc(SortingWorld& world, std::int32_t stat) {
  auto& source = world.SourceRecords();
  auto order = world.Order(kMisc);
  StableOrder(world, order, Extent(kMisc), kOrderBounds,
              [&](std::uint16_t token) {
                return world.MiscValue(
                    stat, Read<Items::InventoryMisc>(source, kMisc, token));
              });
}

bool HasEquipmentProjection(std::int32_t category) {
  return Framework::sorting::HasEquipmentProjection(category);
}

// decomp: sortEquipment (src/MENU/MENUD.PRG/234.c:1529-1548; was
// _sortEquipment); original instructions 80105844..8010592B. Both actual
// callers supply F02968's independent stack output, not byte-overlapping
// container storage.
void CopyEquipmentStats(Items::EquipmentPart& target, std::int32_t category,
                        const Items::ContainerRecord* source,
                        Items::ContainerWorld& records) {
  if (!HasEquipmentProjection(category)) return;
  Framework::sorting::ProjectEquipmentStats(
      target, category, [&]<typename Record>() {
        if (source == nullptr) {
          records.Refuse("missing equipment projection source");
        }
        const auto* typed = std::get_if<Record>(source);
        if (typed == nullptr) {
          records.Refuse("equipment projection source disagrees with category");
        }
        return *typed;
      });
}

// decomp: copyEquipmentByStat (src/MENU/MENUD.PRG/234.c:1550-1600; was
// _copyEquipmentByStat); original instructions 8010592C..80105ACB.
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
  StableOrder(world, order, count, kOrderBounds, [&](std::uint16_t token) {
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
