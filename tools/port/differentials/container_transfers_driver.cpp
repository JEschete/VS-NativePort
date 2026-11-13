#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <type_traits>
#include <vector>

#include "Items/ContainerTransfersWorld.hpp"

namespace {
namespace items = VS::Items;
using Category = items::ItemCategory;
using Bytes = std::vector<std::uint8_t>;
constexpr std::array<std::size_t, 7> kContainerOffsets{
    0, 0xA00, 0x1500, 0x400, 0x1900, 0x2300, 0x3800};
constexpr std::array<std::size_t, 7> kInventoryOffsets{
    0, 0x280, 0x540, 0x100, 0x640, 0x8C0, 0xE00};
constexpr std::array<std::size_t, 7> kSizes{0x4180, 0x3C00, 0x3C00, 0x3C00,
                                            0x4180, 0xF00,  0xB0};

template <typename Storage>
void Decode(Storage &storage, const Bytes &bytes,
            const std::array<std::size_t, 7> &offsets) {
  for (std::size_t category = 0; category < offsets.size(); ++category) {
    items::Live::VisitContainerTable(
        storage, static_cast<Category>(category), [&](auto &table) {
          using Record =
              typename std::remove_reference_t<decltype(table)>::value_type;
          static_assert(std::has_unique_object_representations_v<Record>);
          for (std::size_t slot = 0; slot < table.size(); ++slot) {
            std::array<std::uint8_t, sizeof(Record)> record{};
            std::copy_n(bytes.begin() +
                            static_cast<std::ptrdiff_t>(offsets[category] +
                                                        slot * record.size()),
                        record.size(), record.begin());
            table[slot] = std::bit_cast<Record>(record);
          }
        });
  }
}

template <typename Storage>
void Encode(const Storage &storage, Bytes &bytes,
            const std::array<std::size_t, 7> &offsets) {
  for (std::size_t category = 0; category < offsets.size(); ++category) {
    items::Live::VisitContainerTable(
        storage, static_cast<Category>(category), [&](const auto &table) {
          for (std::size_t slot = 0; slot < table.size(); ++slot) {
            using Record = std::remove_cvref_t<decltype(table[slot])>;
            const auto record =
                std::bit_cast<std::array<std::uint8_t, sizeof(Record)>>(
                    table[slot]);
            std::copy(record.begin(), record.end(),
                      bytes.begin() +
                          static_cast<std::ptrdiff_t>(offsets[category] +
                                                      slot * record.size()));
          }
        });
  }
}

void DecodeOrder(items::ContainerStorage &storage, const Bytes &bytes) {
  for (std::size_t row = 0; row < storage.indices.size(); ++row) {
    storage.indices[row] = static_cast<std::uint16_t>(
        bytes.at(0x3C00 + row * 2) | bytes.at(0x3C01 + row * 2) << 8);
  }
}

void EncodeOrder(const items::ContainerStorage &storage, Bytes &bytes) {
  for (std::size_t row = 0; row < storage.indices.size(); ++row) {
    bytes.at(0x3C00 + row * 2) =
        static_cast<std::uint8_t>(storage.indices[row]);
    bytes.at(0x3C01 + row * 2) =
        static_cast<std::uint8_t>(storage.indices[row] >> 8);
  }
}

std::array<Bytes, 7> ReadBlocks() {
  std::array<Bytes, 7> result;
  for (std::size_t block = 0; block < result.size(); ++block) {
    int active = 0, fill = 0;
    std::size_t patches = 0;
    std::cin >> active >> fill >> patches;
    if (active == 0)
      continue;
    auto &bytes = result[block];
    bytes.assign(kSizes[block], static_cast<std::uint8_t>(fill));
    for (std::size_t number = 0; number < patches; ++number) {
      std::size_t offset = 0, size = 0;
      std::uint32_t value = 0;
      std::cin >> offset >> value >> size;
      for (std::size_t byte = 0; byte < size; ++byte) {
        bytes.at(offset + byte) =
            static_cast<std::uint8_t>(value >> (byte * 8));
      }
    }
  }
  return result;
}

void Run(int kind, int flags, std::int32_t source_slot, int alias) {
  container_transfers_tests::BagWorld bag;
  items::ContainerStorage target;
  items::ContainerStorage source;
  items::ContainerStorage preview;
  items::ContainerStorage staged;
  for (auto &capacity : target.capacities) {
    unsigned value = 0;
    std::cin >> value;
    capacity = static_cast<std::uint16_t>(value);
  }
  bag.storage.capacities = target.capacities;
  preview.capacities = target.capacities;
  for (auto &capacity : bag.capacities) {
    unsigned value = 0;
    std::cin >> value;
    capacity = static_cast<std::uint8_t>(value);
  }
  auto before = ReadBlocks();
  std::size_t pending_size = 0;
  std::cin >> pending_size;
  std::vector<std::uint16_t> pending(pending_size);
  for (auto &token : pending) {
    unsigned value = 0;
    std::cin >> value;
    token = static_cast<std::uint16_t>(value);
  }
  std::array<items::ContainerStorage *, 5> owners{&target, &source, &preview,
                                                  &staged, &bag.storage};
  for (std::size_t block = 0; block < owners.size(); ++block) {
    if (before[block].empty())
      continue;
    Decode(owners[block]->items, before[block], kContainerOffsets);
    if (block == 0 || block == 4)
      DecodeOrder(*owners[block], before[block]);
  }
  if (!before[5].empty())
    Decode(bag.inventory, before[5], kInventoryOffsets);
  if (!before[6].empty()) {
    bag.indices = std::bit_cast<items::InventoryIndices>([&] {
      std::array<std::uint8_t, 0xB0> bytes{};
      std::copy(before[6].begin(), before[6].end(), bytes.begin());
      return bytes;
    }());
  }
  const auto category = static_cast<Category>(flags & 0xF);
  std::int32_t result = 0;
  switch (kind) {
  case 0:
    items::PreviewTransferredMisc(preview, target, staged, pending);
    break;
  case 1:
    result = items::RefreshMiscTransferCount(preview, target, staged, pending);
    break;
  case 2:
    result = items::CopyContainerItem(
        {category,
         (flags & 0x10) != 0 ? items::ContainerTransferMode::kCopy
                             : items::ContainerTransferMode::kCheck,
         flags >> 8},
        target, source_slot, alias != 0 ? target : source);
    break;
  case 3: {
    const auto offset = items::ContainerCategoryIndices(category, target);
    items::RebuildContainerCategoryIndices(
        category, std::span(target.indices).subspan(offset), target);
    break;
  }
  case 4:
    items::SnapshotGadgetBag(bag);
    break;
  case 5:
    items::CommitGadgetBag(bag);
    break;
  case 6:
    result = items::ContainerParentWeaponIndex(static_cast<Category>(flags),
                                               source_slot, target);
    break;
  case 7:
    result = items::ContainerParentShieldIndex(static_cast<Category>(flags),
                                               source_slot, target);
    break;
  default:
    std::terminate();
  }
  auto after = before;
  for (std::size_t block = 0; block < owners.size(); ++block) {
    if (after[block].empty())
      continue;
    Encode(owners[block]->items, after[block], kContainerOffsets);
    if (block == 0 || block == 4)
      EncodeOrder(*owners[block], after[block]);
  }
  if (!after[5].empty())
    Encode(bag.inventory, after[5], kInventoryOffsets);
  if (!after[6].empty()) {
    const auto bytes =
        std::bit_cast<std::array<std::uint8_t, 0xB0>>(bag.indices);
    std::copy(bytes.begin(), bytes.end(), after[6].begin());
  }
  std::size_t changes = 0;
  for (std::size_t block = 0; block < before.size(); ++block) {
    for (std::size_t offset = 0; offset < before[block].size(); ++offset) {
      changes += before[block][offset] != after[block][offset] ? 1U : 0U;
    }
  }
  std::cout << result << ' ' << changes;
  for (std::size_t block = 0; block < before.size(); ++block) {
    for (std::size_t offset = 0; offset < before[block].size(); ++offset) {
      if (before[block][offset] != after[block][offset]) {
        std::cout << ' ' << block << ' ' << offset << ' '
                  << static_cast<unsigned>(after[block][offset]);
      }
    }
  }
  std::cout << '\n';
}
} // namespace

int main() {
  std::ios::sync_with_stdio(false);
  int kind = 0, flags = 0, alias = 0;
  std::int32_t source_slot = 0;
  while (std::cin >> kind >> flags >> source_slot >> alias) {
    Run(kind, flags, source_slot, alias);
  }
}
