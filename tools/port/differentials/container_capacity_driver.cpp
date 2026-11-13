#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <type_traits>
#include <vector>

#include "Items/ContainerGemCounts.hpp"
#include "Items/ContainerTables.hpp"
#include "Menu/Container/ContainerCapacity.hpp"
#include "Menu/ContainerCapacityWorld.hpp"

namespace {
namespace items = VS::Items;
namespace capacity = VS::Menu::container;
using World = container_capacity_tests::World;
using Bytes = std::vector<std::uint8_t>;
constexpr std::size_t kBlockBytes = 0x3C00;
constexpr std::size_t kOwnerCount = 4;
constexpr std::array<std::size_t, 7> kTableOffsets{
    0, 0xA00, 0x1500, 0x400, 0x1900, 0x2300, 0x3800};

std::int64_t Read() {
  std::int64_t value = 0;
  if (!(std::cin >> value))
    std::exit(2);
  return value;
}

template <typename Range> void ReadRange(Range &values) {
  for (auto &value : values) {
    value = static_cast<std::remove_reference_t<decltype(value)>>(Read());
  }
}

Bytes ReadBlock() {
  const auto fill = static_cast<std::uint8_t>(Read());
  const auto count = static_cast<std::size_t>(Read());
  Bytes bytes(kBlockBytes, fill);
  for (std::size_t number = 0; number < count; ++number) {
    const auto offset = static_cast<std::size_t>(Read());
    const auto value = static_cast<std::uint32_t>(Read());
    const auto width = static_cast<std::size_t>(Read());
    for (std::size_t byte = 0; byte < width; ++byte) {
      bytes.at(offset + byte) = static_cast<std::uint8_t>(value >> (byte * 8));
    }
  }
  return bytes;
}

void Decode(items::ContainerStorage &storage, const Bytes &bytes) {
  for (std::size_t category = 0; category < kTableOffsets.size(); ++category) {
    items::Live::VisitContainerTable(
        storage.items, static_cast<items::ItemCategory>(category),
        [&](auto &table) {
          using Record =
              typename std::remove_reference_t<decltype(table)>::value_type;
          static_assert(std::has_unique_object_representations_v<Record>);
          for (std::size_t slot = 0; slot < table.size(); ++slot) {
            std::array<std::uint8_t, sizeof(Record)> record{};
            std::copy_n(bytes.begin() +
                            static_cast<std::ptrdiff_t>(
                                kTableOffsets[category] + slot * record.size()),
                        record.size(), record.begin());
            table[slot] = std::bit_cast<Record>(record);
          }
        });
  }
}

Bytes Encode(const items::ContainerStorage &storage, const Bytes &before) {
  auto bytes = before;
  for (std::size_t category = 0; category < kTableOffsets.size(); ++category) {
    items::Live::VisitContainerTable(
        storage.items, static_cast<items::ItemCategory>(category),
        [&](const auto &table) {
          for (std::size_t slot = 0; slot < table.size(); ++slot) {
            using Record = std::remove_cvref_t<decltype(table[slot])>;
            const auto record =
                std::bit_cast<std::array<std::uint8_t, sizeof(Record)>>(
                    table[slot]);
            std::copy(record.begin(), record.end(),
                      bytes.begin() +
                          static_cast<std::ptrdiff_t>(kTableOffsets[category] +
                                                      slot * record.size()));
          }
        });
  }
  return bytes;
}

struct Input {
  World world;
  int category = static_cast<int>(Read());
  std::int32_t slot = static_cast<std::int32_t>(Read());
  std::int32_t row = static_cast<std::int32_t>(Read());
  std::int32_t additional = static_cast<std::int32_t>(Read());
  std::int32_t limit = static_cast<std::int32_t>(Read());
  std::uint32_t position = static_cast<std::uint32_t>(Read());
  std::int32_t occupied = static_cast<std::int32_t>(Read());
  int direction = static_cast<int>(Read());
  int alias = static_cast<int>(Read());
  bool callback = Read() != 0;
  std::array<items::ContainerStorage *, kOwnerCount> owners{
      &world.source, &world.destination, &world.staged, &world.bag};
  std::array<Bytes, kOwnerCount> before;

  explicit Input(int kind) {
    items::ContainerCapacities capacities{};
    ReadRange(capacities);
    ReadRange(world.bag_limits);
    world.limits = capacities;
    for (auto *owner : owners)
      owner->capacities = capacities;
    for (std::size_t block = 0; block < owners.size(); ++block) {
      before[block] = ReadBlock();
      Decode(*owners[block], before[block]);
    }
    ReadRange(world.pending);
    if (kind >= 4 && direction == 0)
      world.limits = world.bag_limits;
    if (alias == 1)
      world.receiving = &world.source;
    if (alias == 2)
      world.active = &world.staged;
    if (alias == 3)
      world.receiving = &world.staged;
    if (callback) {
      world.number_callback = [this] {
        constexpr auto kMisc =
            static_cast<std::size_t>(items::ItemCategory::kMisc);
        constexpr std::uint16_t kCallbackLimit = 201;
        world.bag_limits.at(kMisc) = kCallbackLimit;
      };
    }
  }

  std::int32_t Run(int kind) {
    const auto item = capacity::TransferItem{
        static_cast<items::ItemCategory>(category), slot};
    switch (kind) {
    case 0:
      capacity::DrawCapacityCount(
          world, {.limit = limit, .position = position, .occupied = occupied});
      break;
    case 1:
      capacity::DrawStorageCapacities(world);
      break;
    case 2:
      return items::CountContainerWeaponGems(slot, world.Active());
    case 3:
      return items::CountContainerShieldGems(slot, world.Active());
    case 4:
      return capacity::WouldExceedCapacity(item.category, additional, world)
                 ? 1
                 : 0;
    case 5:
      return static_cast<int>(capacity::TransferCapacityError(item, world));
    case 6:
      capacity::UndoPendingTransfer(row, world);
      break;
    case 7:
      capacity::StageItemTransfer(item, world);
      break;
    default:
      std::terminate();
    }
    return 0;
  }

  void Print(std::int32_t result) const {
    std::cout << result << ' ' << world.draws.size();
    for (const auto &draw : world.draws) {
      std::cout << ' ' << (draw.number ? 1 : 0) << ' ' << draw.value.value
                << ' ' << draw.value.position << ' ' << draw.value.color << ' '
                << draw.text.size();
      for (const auto letter : draw.text) {
        std::cout << ' '
                  << static_cast<unsigned>(static_cast<std::uint8_t>(letter));
      }
    }
    std::vector<std::array<std::size_t, 3>> changes;
    for (std::size_t block = 0; block < owners.size(); ++block) {
      const auto bytes = Encode(*owners[block], before[block]);
      for (std::size_t offset = 0; offset < bytes.size(); ++offset) {
        if (bytes[offset] != before[block][offset]) {
          changes.push_back({block, offset, bytes[offset]});
        }
      }
    }
    std::cout << ' ' << changes.size();
    for (const auto &change : changes) {
      for (const auto value : change)
        std::cout << ' ' << value;
    }
    for (const auto token : world.pending)
      std::cout << ' ' << token;
    std::cout << '\n';
  }
};
} // namespace

int main() {
  std::ios::sync_with_stdio(false);
  int kind = 0;
  while (std::cin >> kind) {
    Input input(kind);
    const auto result = input.Run(kind);
    input.Print(result);
  }
}
