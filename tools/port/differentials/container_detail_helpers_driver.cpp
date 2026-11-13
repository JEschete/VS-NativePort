#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <type_traits>
#include <vector>

#include "Items/ContainerTables.hpp"
#include "Menu/ContainerDetailHelpersWorld.hpp"

namespace {
namespace detail = VS::Menu::container;
namespace items = VS::Items;
namespace fw = VS::Menu::Framework;
using container_detail_helpers_tests::Event;
using container_detail_helpers_tests::World;
using Byte = std::uint8_t;
constexpr std::size_t kDiskRowBytes = 64, kDiskRowTextEnd = 60;
constexpr std::size_t kContainerBytes = 0x3C00, kCursorBytes = 0xF2;
constexpr std::array<std::size_t, 7> kTableOffsets{
    0, 0xA00, 0x1500, 0x400, 0x1900, 0x2300, 0x3800};
constexpr char kBorrowed[] = "borrowed";
static_assert(offsetof(fw::MenuRow, text) == 28);
static_assert(std::is_trivially_copyable_v<fw::MenuRow>);

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

template <typename Value> Value ReadRecord() {
  std::array<Byte, sizeof(Value)> bytes{};
  ReadRange(bytes);
  Value value{};
  std::memcpy(&value, bytes.data(), bytes.size());
  return value;
}

template <typename Value> void PrintRecord(const Value &value) {
  std::array<Byte, sizeof(Value)> bytes{};
  std::memcpy(bytes.data(), &value, bytes.size());
  for (const auto byte : bytes)
    std::cout << static_cast<unsigned>(byte) << ' ';
  std::cout << '\n';
}

void ReadContents(items::ContainerStorage &storage) {
  std::array<Byte, kContainerBytes> bytes{};
  ReadRange(bytes);
  for (std::size_t category = 0; category < kTableOffsets.size(); ++category) {
    items::Live::VisitContainerTable(
        storage.items, static_cast<items::ItemCategory>(category),
        [&](auto &table) {
          std::memcpy(table.data(), bytes.data() + kTableOffsets[category],
                      table.size() * sizeof(table[0]));
        });
  }
}

void Stats(int kind) {
  const bool present = Read() != 0;
  static_cast<void>(Read());
  items::ContainerStorage records;
  if (kind == 0) {
    auto target = ReadRecord<items::WeaponLoadout>();
    const auto source = ReadRecord<items::InventoryWeapon>();
    ReadContents(records);
    static_cast<void>(
        detail::BuildWeaponStats(target, present ? &source : nullptr, records));
    PrintRecord(target);
  } else {
    auto target = ReadRecord<items::ShieldLoadout>();
    const auto source = ReadRecord<items::InventoryShield>();
    ReadContents(records);
    detail::BuildShieldStats(target, present ? &source : nullptr, records);
    PrintRecord(target);
  }
}

struct RowImage {
  std::array<Byte, kDiskRowBytes> before{};
  std::uint32_t text_token = 0;

  void Load(fw::MenuRow &row) {
    ReadRange(before);
    std::memcpy(&row, before.data(), kDiskRowTextEnd);
    std::memcpy(&text_token, before.data() + kDiskRowTextEnd,
                sizeof(text_token));
    row.sub_text = text_token == 0 ? nullptr : kBorrowed;
  }
  void Print(const fw::MenuRow &row) const {
    auto after = before;
    std::memcpy(after.data(), &row, kDiskRowTextEnd);
    const auto token = row.sub_text == nullptr ? 0U : text_token;
    std::memcpy(after.data() + kDiskRowTextEnd, &token, sizeof(token));
    for (const auto byte : after) {
      std::cout << ' ' << static_cast<unsigned>(byte);
    }
  }
};

struct PolicyInput {
  World world;
  int first = 0, second = 0, mode = 0, row_reads = 0;
  std::uint32_t after_repeat = 0, after_pressed = 0;
  std::array<Byte, kCursorBytes> memory{};
  std::array<std::uint16_t, items::kContainerIndexEntries> order{};
  items::ContainerStorage indexing;
  std::array<std::array<RowImage, fw::kRowCount>, 2> images{};
  std::vector<std::array<std::int64_t, 8>> trace;

  void ReadInput(int kind) {
    first = static_cast<int>(Read());
    second = static_cast<int>(Read());
    world.count = static_cast<int>(Read());
    world.held = static_cast<std::uint32_t>(Read());
    world.repeat = static_cast<std::uint32_t>(Read());
    world.pressed = static_cast<std::uint32_t>(Read());
    after_repeat = static_cast<std::uint32_t>(Read());
    after_pressed = static_cast<std::uint32_t>(Read());
    mode = static_cast<int>(Read());
    world.panels = {static_cast<Byte>(Read()), static_cast<Byte>(Read()),
                    static_cast<Byte>(Read())};
    world.phase = static_cast<Byte>(Read());
    world.triangle = static_cast<std::int32_t>(Read());
    if (kind == 6) {
      ReadRange(memory);
      ReadRange(indexing.capacities);
      ReadRange(order);
    }
    if (kind == 4) {
      for (std::size_t owner = 0; owner < images.size(); ++owner) {
        for (std::size_t row = 0; row < fw::kRowCount; ++row) {
          images[owner][row].Load(world.rows[owner][row]);
        }
      }
    }
    Bind(kind);
  }

  void Bind(int kind) {
    world.on_call = [this, kind](Event event, std::int32_t argument) {
      if (event == Event::kCount)
        argument = first;
      trace.push_back({static_cast<int>(event), argument,
                       event == Event::kCount ? 1 : 0, world.panels.title,
                       world.panels.icons, world.panels.controls, world.phase,
                       world.triangle});
      if (event == Event::kCount) {
        world.repeat = after_repeat;
        world.pressed = after_pressed;
      }
      if (event == Event::kClear && kind == 4 && mode == 1)
        world.owner = 1;
      if (event == Event::kRow && mode == 2 && ++row_reads == 1) {
        world.owner = 1;
      }
      if (event == Event::kPrimary && mode == 1)
        world.pressed = after_pressed;
      if (event == Event::kPrimary && mode == 2) {
        world.panels.icons = 0x93;
        world.panels.controls = 0xA7;
      }
    };
    world.on_remember = [this](detail::DetailCursor cursor) {
      const auto offset = 0xA2 + static_cast<std::size_t>(first) * 2;
      memory.at(offset) = cursor.row;
      memory.at(offset + 1) = cursor.page;
    };
    world.on_order = [this] {
      const auto offset = items::ContainerCategoryIndices(
          static_cast<items::ItemCategory>(first), indexing);
      std::size_t end = 0;
      for (const auto capacity : items::kContainerCapacities) {
        end += capacity;
        if (end > offset)
          break;
      }
      return std::span<const std::uint16_t>(order).subspan(offset,
                                                           end - offset);
    };
  }

  void Print(int kind, std::int32_t result) const {
    std::cout << result << ' ' << static_cast<unsigned>(world.panels.title)
              << ' ' << static_cast<unsigned>(world.panels.icons) << ' '
              << static_cast<unsigned>(world.panels.controls) << ' '
              << static_cast<unsigned>(world.phase) << ' ' << world.triangle;
    if (kind == 6) {
      for (const auto byte : memory) {
        std::cout << ' ' << static_cast<unsigned>(byte);
      }
    }
    if (kind == 4) {
      for (std::size_t owner = 0; owner < images.size(); ++owner) {
        for (std::size_t row = 0; row < fw::kRowCount; ++row) {
          images[owner][row].Print(world.rows[owner][row]);
        }
      }
    }
    std::cout << ' ' << trace.size();
    for (const auto &call : trace) {
      for (const auto value : call)
        std::cout << ' ' << value;
    }
    std::cout << '\n';
  }
};

void Policy(int kind) {
  PolicyInput input;
  input.ReadInput(kind);
  auto &world = input.world;
  std::int32_t result = 0;
  switch (kind) {
  case 2:
    detail::SetPanelVisibility(world.panels, static_cast<Byte>(input.first));
    break;
  case 3:
    result = detail::PollDetailsPaging(world, {input.second}).index;
    break;
  case 4:
    detail::BeginDetailsTransition(world, {input.first});
    break;
  case 5:
    detail::InitializeDetailsLayout(world, {input.first}, input.second);
    break;
  case 6:
    result = detail::SelectDetailsItem(world, {input.second});
    break;
  case 7:
    detail::ExitDetailsView(world, input.first != 0);
    break;
  default:
    std::exit(4);
  }
  input.Print(kind, result);
}
} // namespace

int main() {
  int kind = 0;
  while (std::cin >> kind) {
    if (kind < 2)
      Stats(kind);
    else
      Policy(kind);
  }
}
