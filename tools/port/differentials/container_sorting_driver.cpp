#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <type_traits>
#include <variant>

#include "Items/ContainerTables.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/ContainerSortingWorld.hpp"
#include "container_sorting_keys.hpp"

namespace {
namespace sorting = VS::Menu::container;
namespace fixture = sorting::sort_test;
namespace items = VS::Items;
constexpr std::size_t kTargetBytes = sizeof(items::EquipmentPart);
using RecordBytes = std::array<std::uint8_t, sizeof(Items_ContainerData)>;

class World final : public fixture::World {
public:
  std::int32_t WeaponValue(std::int32_t stat,
                           const items::WeaponLoadout &weapon) override {
    return Observe(fixture::Operation::kWeapon, stat,
                   keys_.Weapon(stat, weapon));
  }
  std::int32_t ShieldValue(std::int32_t stat,
                           const items::ShieldLoadout &shield) override {
    return Observe(fixture::Operation::kShield, stat,
                   keys_.Shield(stat, shield));
  }
  std::int32_t MiscValue(std::int32_t stat,
                         const items::InventoryMisc &item) override {
    return Observe(fixture::Operation::kMisc, stat, keys_.Misc(stat, item));
  }
  std::int32_t EquipmentValue(std::int32_t stat,
                              const items::EquipmentPart &item) override {
    return Observe(fixture::Operation::kEquipment, stat,
                   keys_.Equipment(stat, item));
  }

private:
  fixture::ProductionKeys keys_;
};

template <typename Range> void Read(Range &values) {
  for (auto &value : values) {
    std::int64_t number = 0;
    std::cin >> number;
    value = static_cast<std::remove_reference_t<decltype(value)>>(number);
  }
}

template <typename Range> void Print(const Range &values) {
  for (const auto value : values)
    std::cout << +value << ' ';
}

void Load(items::ContainerStorage &records, const RecordBytes &bytes) {
  const auto legacy = std::bit_cast<Items_ContainerData>(bytes);
  for (std::size_t value = 0;
       value < static_cast<std::size_t>(items::ItemCategory::kManage);
       ++value) {
    const auto category = static_cast<items::ItemCategory>(value);
    items::Live::VisitContainerTable(legacy, category, [&](const auto &table) {
      for (std::size_t row = 0; row < std::size(table); ++row) {
        records.Write(category, row, items::Live::Typed(table[row]));
      }
    });
  }
}

std::uint32_t Checksum(items::ContainerStorage &records) {
  Items_ContainerData legacy{};
  for (std::size_t value = 0;
       value < static_cast<std::size_t>(items::ItemCategory::kManage);
       ++value) {
    const auto category = static_cast<items::ItemCategory>(value);
    items::Live::VisitContainerTable(legacy, category, [&](auto &table) {
      using Legacy = std::remove_reference_t<decltype(table[0])>;
      for (std::size_t row = 0; row < std::size(table); ++row) {
        const auto record = records.Read(category, row);
        const auto *typed =
            std::get_if<items::Live::TypedRecordOf<Legacy>>(&record);
        if (typed == nullptr)
          std::abort();
        items::Live::Commit(table[row], *typed);
      }
    });
  }
  const auto bytes = std::bit_cast<RecordBytes>(legacy);
  std::uint32_t checksum = 0;
  for (std::size_t position = 0; position < bytes.size(); ++position) {
    checksum += static_cast<std::uint32_t>(position + 1) * bytes[position];
  }
  return checksum;
}

void Run(World &world, int kind, int category, int stat, std::size_t index,
         items::EquipmentPart &target) {
  switch (kind) {
  case 0:
    sorting::SortWeapons(world, stat);
    return;
  case 1:
    sorting::SortShields(world, stat);
    return;
  case 2:
    sorting::SortMisc(world, stat);
    return;
  case 3: {
    std::optional<items::ContainerRecord> source;
    if (sorting::HasEquipmentProjection(category)) {
      source = world.owners[world.owner].Read(
          static_cast<items::ItemCategory>(category), index);
    }
    sorting::CopyEquipmentStats(target, category,
                                source.has_value() ? &source.value() : nullptr,
                                world.owners[world.owner]);
    return;
  }
  default:
    sorting::SortEquipment(world, category, stat);
    return;
  }
}
} // namespace

int main() {
  int kind = 0;
  int category = 0;
  int stat = 0;
  std::size_t index = 0;
  int mode = 0;
  std::size_t count = 0;
  while (std::cin >> kind >> category >> stat >> index >> mode >> count) {
    World world;
    items::ContainerCapacities capacities{};
    Read(capacities);
    for (auto &owner : world.owners) {
      RecordBytes bytes{};
      Read(bytes);
      Load(owner, bytes);
      owner.capacities = capacities;
      Read(owner.indices);
    }
    std::array<std::uint8_t, kTargetBytes> target_bytes{};
    Read(target_bytes);
    auto target = std::bit_cast<items::EquipmentPart>(target_bytes);
    world.trace_count = count;
    std::size_t keys = 0;
    world.hook = [&](auto &state, auto operation) {
      if (operation == fixture::Operation::kOrder) {
        if (mode == 2)
          state.owner = 1;
        if (mode == 3) {
          for (auto &owner : state.owners)
            owner.capacities[category] = 1;
        }
      }
      if (operation >= fixture::Operation::kWeapon && ++keys == 1 &&
          mode == 1) {
        state.owner = 1;
      }
    };
    Run(world, kind, category, stat, index, target);
    std::cout << world.owner << ' ';
    Print(world.owners[0].capacities);
    for (auto &owner : world.owners)
      std::cout << Checksum(owner) << ' ';
    Print(std::bit_cast<std::array<std::uint8_t, kTargetBytes>>(target));
    for (const auto &owner : world.owners)
      Print(owner.indices);
    std::cout << keys << ' ';
    for (const auto &event : world.events) {
      if (event.operation >= fixture::Operation::kWeapon) {
        std::cout << kind << ' ' << event.stat << ' ' << event.value << ' '
                  << event.order_hash << ' ' << event.owner << ' ';
      }
    }
    std::cout << '\n';
  }
}
