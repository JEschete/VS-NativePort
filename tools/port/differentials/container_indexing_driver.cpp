#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "Items/ContainerIndexing.hpp"
#include "Items/ContainerTables.hpp"

namespace {
namespace items = VS::Items;
using Category = items::ItemCategory;
constexpr std::array<std::size_t, 7> kOffsets{0,      0xA00,  0x1500, 0x400,
                                              0x1900, 0x2300, 0x3800};
using Bytes = std::array<std::uint8_t, 0x3C00>;

void ReadStorage(items::ContainerStorage &storage, const Bytes &bytes) {
  for (std::size_t category = 0; category < kOffsets.size(); ++category) {
    items::Live::VisitContainerTable(
        storage.items, static_cast<Category>(category), [&](auto &table) {
          using Record =
              typename std::remove_reference_t<decltype(table)>::value_type;
          for (std::size_t i = 0; i < table.size(); ++i) {
            std::array<std::uint8_t, sizeof(Record)> record{};
            std::copy_n(bytes.begin() +
                            static_cast<std::ptrdiff_t>(kOffsets[category] +
                                                        i * sizeof(Record)),
                        record.size(), record.begin());
            table[i] = std::bit_cast<Record>(record);
          }
        });
  }
}

Bytes WriteStorage(items::ContainerStorage &storage) {
  Bytes bytes{};
  for (std::size_t category = 0; category < kOffsets.size(); ++category) {
    items::Live::VisitContainerTable(
        storage.items, static_cast<Category>(category), [&](const auto &table) {
          for (std::size_t i = 0; i < table.size(); ++i) {
            using Record = std::remove_cvref_t<decltype(table[i])>;
            const auto record =
                std::bit_cast<std::array<std::uint8_t, sizeof(Record)>>(
                    table[i]);
            std::copy(record.begin(), record.end(),
                      bytes.begin() +
                          static_cast<std::ptrdiff_t>(kOffsets[category] +
                                                      i * sizeof(Record)));
          }
        });
  }
  return bytes;
}

void Run(int kind, int category, std::int32_t index, int fill) {
  items::ContainerStorage storage;
  for (auto &capacity : storage.capacities) {
    unsigned value = 0;
    std::cin >> value;
    capacity = static_cast<std::uint16_t>(value);
  }
  Bytes before{};
  before.fill(static_cast<std::uint8_t>(fill));
  std::size_t patches = 0;
  std::cin >> patches;
  for (std::size_t i = 0; i < patches; ++i) {
    std::size_t offset = 0, size = 0;
    std::uint32_t value = 0;
    std::cin >> offset >> value >> size;
    for (std::size_t n = 0; n < size; ++n) {
      before.at(offset + n) = static_cast<std::uint8_t>(value >> (n * 8));
    }
  }
  std::size_t order_size = 0;
  std::cin >> order_size;
  std::vector<std::uint16_t> order(order_size);
  for (auto &token : order) {
    unsigned value = 0;
    std::cin >> value;
    token = static_cast<std::uint16_t>(value);
  }
  if (kind >= 3)
    ReadStorage(storage, before);
  const auto typed_category = static_cast<Category>(category);
  std::int64_t result = 0;
  switch (kind) {
  case 0:
    result = static_cast<std::int64_t>(
        items::ContainerCategoryIndices(typed_category, storage) * 2);
    break;
  case 1:
    result = items::FindContainerItemRow(typed_category, index, order, storage);
    break;
  case 2:
    items::AppendContainerItemIndex(typed_category, index, order, storage);
    break;
  case 3:
    items::ClearContainerItem(typed_category, index, storage);
    break;
  case 4:
    result = items::ContainerItemId(typed_category, index, storage);
    break;
  case 5:
    result = items::CountContainerItems(typed_category, storage);
    break;
  case 6:
    result = items::FindEmptyContainerSlot(typed_category, storage);
    break;
  default:
    std::terminate();
  }
  if (kind != 2)
    order.clear();
  std::vector<std::pair<std::size_t, unsigned>> changes;
  if (kind == 3) {
    const auto after = WriteStorage(storage);
    for (std::size_t offset = 0; offset < after.size(); ++offset) {
      if (after[offset] != before[offset])
        changes.emplace_back(offset, after[offset]);
    }
  }
  std::cout << result << ' ' << order.size();
  for (auto token : order)
    std::cout << ' ' << token;
  std::cout << ' ' << changes.size();
  for (auto [offset, value] : changes)
    std::cout << ' ' << offset << ' ' << value;
  std::cout << '\n';
}
} // namespace

int main() {
  std::ios::sync_with_stdio(false);
  int kind = 0, category = 0, fill = 0;
  std::int32_t index = 0;
  while (std::cin >> kind >> category >> index >> fill)
    Run(kind, category, index, fill);
}
