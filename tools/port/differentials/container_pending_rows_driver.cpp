#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <span>
#include <type_traits>

#include "Items/ContainerTables.hpp"
#include "Menu/ContainerPendingRowsWorld.hpp"

namespace {
namespace tests = container_pending_rows_tests;
namespace rows = VS::Menu::container;
namespace items = VS::Items;
namespace framework = VS::Menu::Framework;
constexpr std::array<std::size_t, 7> kOffsets{0,      0xA00,  0x1500, 0x400,
                                              0x1900, 0x2300, 0x3800};
constexpr std::size_t kBlockBytes = 0x3C00;

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

void ReadRecords(tests::Records &records) {
  std::array<std::uint8_t, kBlockBytes> bytes{};
  ReadRange(bytes);
  for (std::size_t category = 0; category < kOffsets.size(); ++category) {
    items::Live::VisitContainerTable(
        records.storage.items, static_cast<items::ItemCategory>(category),
        [&](auto &table) {
          std::memcpy(table.data(), bytes.data() + kOffsets[category],
                      table.size() * sizeof(table[0]));
        });
  }
}

void PrintText(const tests::StoredRow &row,
               const std::optional<framework::TextRef> &text, std::size_t index,
               bool description, const tests::World &world) {
  if (!row.stored || !text) {
    std::cout << " 0 " << 0x50000000U + index * 8U + (description ? 4U : 0U);
    return;
  }
  auto source = static_cast<unsigned>(text->source);
  auto offset = text->offset;
  if (text->source == framework::TextSource::kBuffer) {
    offset += index * framework::kRowDescriptionBufferBytes;
  } else if (text->source == framework::TextSource::kWeaponName) {
    source = row.records == &world.first ? 5 : 6;
    offset =
        static_cast<std::size_t>(row.slot) * sizeof(items::InventoryWeapon) +
        offsetof(items::InventoryWeapon, name);
  }
  std::cout << ' ' << source << ' ' << offset;
}

void Print(int kind, std::int32_t result, const tests::World &world) {
  std::cout << result;
  for (const auto token : world.pending)
    std::cout << ' ' << token;
  if (kind == 3) {
    for (const auto type : world.types)
      std::cout << ' ' << type;
    for (std::size_t index = 0; index < world.output.size(); ++index) {
      const auto &row = world.output[index];
      PrintText(row, row.text.name, index, false, world);
      PrintText(row, row.text.description, index, true, world);
    }
    for (const auto &buffer : world.descriptions) {
      for (const auto byte : buffer)
        std::cout << ' ' << static_cast<unsigned>(byte);
    }
  }
  std::cout << '\n';
}

void Run(int kind) {
  tests::World world;
  const auto category = static_cast<std::int32_t>(Read());
  const auto slot = static_cast<std::int32_t>(Read());
  const auto staged = Read();
  const auto active = Read();
  const auto fill = static_cast<std::uint8_t>(Read());
  ReadRange(world.limits);
  ReadRange(world.first_pending);
  if (kind >= 2) {
    ReadRecords(world.first);
    ReadRecords(world.second);
  }
  world.staging = staged == 0 ? &world.first : &world.second;
  world.active = active == 0 ? &world.first : &world.second;
  if (kind == 3) {
    ReadRange(world.order);
    ReadRange(world.types);
    for (auto &buffer : world.descriptions)
      buffer.fill(fill);
    if (active == staged && category != 7) {
      world.pending =
          std::span(world.order).first(rows::kPendingTransferEntries);
    }
  }
  const auto first = world.first.storage.items;
  const auto second = world.second.storage.items;
  std::int32_t result = 0;
  switch (kind) {
  case 0:
    result = rows::FindPendingTransfer({category, slot}, world);
    break;
  case 1:
    rows::AppendPendingTransfer({category, slot}, world);
    break;
  case 2:
    rows::RebuildPendingTransfers(world);
    break;
  case 3:
    result =
        rows::BuildItemRows(static_cast<items::ItemCategory>(category), world);
    break;
  default:
    std::exit(2);
  }
  if (world.first.storage.items != first ||
      world.second.storage.items != second) {
    throw std::runtime_error("native operation changed its source records");
  }
  Print(kind, result, world);
}
} // namespace

int main() {
  int kind = 0;
  std::size_t vector = 0;
  try {
    while (std::cin >> kind) {
      Run(kind);
      ++vector;
    }
  } catch (const std::exception &error) {
    std::cerr << "vector " << vector << ": " << error.what() << '\n';
    return 3;
  }
}
