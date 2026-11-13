#include "Menu/Container/ContainerSortingLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <string_view>

#include "Items/ContainerIndexingLive.hpp"
#include "Items/ContainerTables.hpp"
#include "Items/ContainerTransfersLive.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Container/ContainerSorting.hpp"
#include "Menu/Framework/FrameworkLive.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Menu/Container/ItemView.h"
}

namespace VS::Menu::container {
namespace {
[[noreturn]] void Refuse(std::string_view reason) {
  Core_Errorf("menu.container.sorting", "Container sorting refused: %.*s",
              static_cast<int>(reason.size()), reason.data());
  std::abort();
}

Items_ContainerData* CurrentData() {
  return Menu_Container_containerData == nullptr
             ? nullptr
             : &Menu_Container_containerData->items;
}

bool Overlap(const void* first, std::size_t first_bytes, const void* second,
             std::size_t second_bytes) {
  const auto left = reinterpret_cast<std::uintptr_t>(first);
  const auto right = reinterpret_cast<std::uintptr_t>(second);
  return left <= right ? right - left < first_bytes
                       : left - right < second_bytes;
}

class LiveSortingWorld final : public SortingWorld {
 public:
  Items::ContainerWorld& SourceRecords() override {
    source_.emplace(CurrentData());
    return source_.value();
  }
  Items::ContainerWorld& CurrentRecords() override {
    current_.emplace(CurrentData());
    return current_.value();
  }
  std::span<std::uint16_t> Order(Items::ItemCategory category) override {
    if (Menu_Container_containerData == nullptr) {
      Items::LiveContainerWorld capacities;
      static_cast<void>(Items::ContainerCategoryIndices(category, capacities));
      return {};
    }
    return Items::LiveContainerCategoryOrder(category,
                                             Menu_Container_containerData);
  }
  std::uint16_t Capacity(Items::ItemCategory category) override {
    Items::LiveContainerWorld capacities;
    return capacities.Capacity(category);
  }
  std::int32_t WeaponValue(std::int32_t stat,
                           const Items::WeaponLoadout& weapon) override {
    Items_WeaponLoadout legacy{};
    Items::Live::Commit(legacy, weapon);
    return Framework::LiveWeaponSortValue(stat, &legacy);
  }
  std::int32_t ShieldValue(std::int32_t stat,
                           const Items::ShieldLoadout& shield) override {
    Items_ShieldLoadout legacy{};
    Items::Live::Commit(legacy, shield);
    return Framework::LiveShieldSortValue(stat, &legacy);
  }
  std::int32_t MiscValue(std::int32_t stat,
                         const Items::InventoryMisc& item) override {
    Items_InventoryMisc legacy{};
    Items::Live::Commit(legacy, item);
    return Framework::LiveMiscSortValue(stat, &legacy);
  }
  std::int32_t EquipmentValue(std::int32_t stat,
                              const Items::EquipmentPart& part) override {
    Items_EquipmentPartStats legacy{};
    Items::Live::Commit(legacy, part);
    return Framework::LiveEquipmentSortValue(stat, &legacy);
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    container::Refuse(reason);
  }

 private:
  std::optional<Items::LiveContainerWorld> source_;
  std::optional<Items::LiveContainerWorld> current_;
};
}  // namespace

// decomp: _sortWeaponsByStat (MENU/MENUD.PRG/234.c:1306).
void SortLiveWeapons(int stat) {
  LiveSortingWorld world;
  SortWeapons(world, stat);
}

// decomp: _sortShieldsByStat (MENU/MENUD.PRG/234.c:1388).
void SortLiveShields(int stat) {
  LiveSortingWorld world;
  SortShields(world, stat);
}

// decomp: _sortItemsByStat (MENU/MENUD.PRG/234.c:1445).
void SortLiveMisc(int stat) {
  LiveSortingWorld world;
  SortMisc(world, stat);
}

// decomp: _sortEquipment (MENU/MENUD.PRG/234.c:1528).
void CopyLiveEquipmentStats(Items_EquipmentPartStats* target, int category,
                            int index) {
  if (!HasEquipmentProjection(category)) return;
  const auto selected = static_cast<Items::ItemCategory>(category);
  auto* owner = CurrentData();
  Items::LiveContainerWorld records(owner);
  if (index < 0) records.Refuse("negative equipment projection source index");
  const auto record = records.Read(selected, static_cast<std::size_t>(index));
  if (target == nullptr) Refuse("missing equipment projection output");
  if (owner == nullptr) Refuse("missing equipment projection source owner");
  Items::Live::VisitContainerTable(*owner, selected, [&](const auto& table) {
    if (static_cast<std::size_t>(index) >= std::size(table)) {
      Refuse("equipment projection index outside current source table");
    }
    if (Overlap(target, sizeof(*target), &table[index], sizeof(table[index]))) {
      Refuse("byte-overlapped equipment projection source and output");
    }
  });
  auto output = Items::Live::Typed(*target);
  CopyEquipmentStats(output, category, &record, records);
  Items::Live::Commit(*target, output);
}

// decomp: _copyEquipmentByStat (MENU/MENUD.PRG/234.c:1549).
void SortLiveEquipment(int category, int stat) {
  LiveSortingWorld world;
  SortEquipment(world, category, stat);
}
}  // namespace VS::Menu::container
