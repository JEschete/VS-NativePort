#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>

#include "Core/Log.hpp"
#include "Menu/Inventory/InventorySortingLive.hpp"
#include "Menu/InventorySortingLiveFixture.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Boot/OverlayReset.h"
#include "Render/Frame.h"
}

namespace {
namespace sorting = VS::Menu::Inventory;
namespace fixture = sorting::sorting_test;
namespace items = VS::Items;
constexpr int kObservedRefusal = 85;
constexpr int kFailedControl = 86;
constexpr int kStrength = 8;
constexpr std::int8_t kLow = -128;
constexpr std::int8_t kHigh = 127;
constexpr std::uint8_t kId = 221;
constexpr std::uint8_t kWideToken = 255;
constexpr std::uint8_t kFlaggedGem = items::kGemSlots + 128;
using RecordBytes = std::array<std::byte, sizeof(Items_Inventory)>;
using IndexBytes = std::array<std::byte, sizeof(Items_InventoryIndices)>;

void Setup() {
  items::InventoryContents records{};
  records.weapons.front().blade = 1;
  records.weapons.back().blade = items::kBladeSlots;
  records.shields.front().base.strength = kLow;
  records.shields.back().base.strength = kHigh;
  records.shields.back().gems[0] = kFlaggedGem;
  records.misc.front().id = 2;
  records.misc.front().count = 1;
  records.misc.back().id = 1;
  records.misc.back().count = kWideToken;
  const auto fill = [](auto &table) {
    table.front().id = kId;
    table.front().strength = kLow;
    table.back().id = kId;
    table.back().strength = kHigh;
  };
  fill(records.blades);
  fill(records.grips);
  fill(records.armor);
  fill(records.gems);
  records.gems.back().strength = 0;
  items::Live::Commit(Audio_Seq_unusedInventory, records);
  for (auto order : fixture::LiveOrders()) {
    order[0] = 1;
    order[1] = static_cast<std::uint8_t>(order.size());
    order[2] = order[1];
  }
}

void Invoke(int identifier, int category, int stat,
            Items_EquipmentPartStats *target, int index) {
  switch (identifier) {
  case 2583:
    sorting::SortLiveWeapons(stat);
    return;
  case 2585:
    sorting::SortLiveShields(stat);
    return;
  case 2587:
    sorting::SortLiveMisc(stat);
    return;
  case 2589:
    sorting::CopyLiveEquipmentStats(target, category, index);
    return;
  default:
    sorting::SortLiveEquipment(category, stat);
    return;
  }
}
} // namespace

int main(int argc, char **argv) {
  if (argc < 5)
    return kFailedControl;
  const int identifier = std::atoi(argv[1]);
  const std::string mode = argv[2];
  int category = std::atoi(argv[3]);
  if (!VS::Core::SetLogFile(argv[4]))
    return kFailedControl;
  fixture::LiveFixture state;
  Setup();
  auto order = fixture::LiveOrders()[category];
  int index = static_cast<int>(order.size()) - 1;
  int stat = identifier == 2587 ? 1 : kStrength;
  Items_EquipmentPartStats output{};
  output.currentDp = std::numeric_limits<std::uint16_t>::max();
  auto *target = &output;
  std::string expected;
  std::size_t framework_warnings = 0;
  bool partial = false;
  std::array<std::uint8_t, items::kMiscSlots> foreign{};
  if (category == static_cast<int>(items::ItemCategory::kGem)) {
    std::span(Audio_Seq_unusedInventory.gems).back().strength = kHigh;
  }
  if (mode == "empty") {
    std::ranges::fill(order, 0);
    stat = std::numeric_limits<int>::min();
  } else if (mode == "bounds" || mode == "wide-token") {
    if (identifier == 2589) {
      index = mode == "wide-token" ? kWideToken : index + 1;
    } else {
      order[0] = mode == "wide-token" ? kWideToken : order.size() + 1;
    }
    expected = "sort source index outside inventory table";
  } else if (mode == "negative-index") {
    index = -1;
    expected = "sort source index outside inventory table";
  } else if (mode == "missing-output") {
    target = nullptr;
    expected = "missing inventory projection output";
  } else if (mode == "unsupported") {
    target = nullptr;
    category = 257;
    index = std::numeric_limits<int>::min();
  } else if (mode == "missing-order") {
    Menu_Inventory_inventoryIndices[category] = nullptr;
    expected = "missing inventory equipment order";
  } else if (mode == "foreign-order") {
    Menu_Inventory_inventoryIndices[category] = foreign.data();
    expected = "equipment order is outside known inventory index arrays";
  } else if (mode == "short-order") {
    Menu_Inventory_inventoryIndices[category] =
        Audio_Seq_unusedInventoryIndices.weapons;
    expected = "sort row outside captured inventory order";
  } else if (mode == "fixed-route") {
    Menu_Inventory_inventoryIndices[category] = nullptr;
  } else if (mode == "known-alias") {
    auto alias = fixture::LiveOrders().back();
    std::ranges::fill(alias, 0);
    std::copy_n(order.begin(), 3, alias.begin());
    Menu_Inventory_inventoryIndices[category] = alias.data();
    order = alias.first(order.size());
  } else if (mode == "category-low" || mode == "category-high") {
    category = mode == "category-low" ? -1 : std::numeric_limits<int>::max();
    expected = "sort category outside inventory order table";
  } else if (mode == "bad-blade") {
    Audio_Seq_unusedInventory.weapons[0].blade = items::kBladeSlots + 1;
    expected = "part reference outside its inventory table";
  } else if (mode == "bad-grip") {
    Audio_Seq_unusedInventory.weapons[0].grip = items::kGripSlots + 1;
    expected = "part reference outside its inventory table";
  } else if (mode == "bad-gem") {
    if (identifier == 2583) {
      Audio_Seq_unusedInventory.weapons[0].gems[0] = items::kGemSlots + 1;
    } else {
      Audio_Seq_unusedInventory.shields[0].gems[0] = kWideToken;
    }
    expected = "part reference outside its inventory table";
  } else if (mode.starts_with("overlap")) {
    auto address = reinterpret_cast<std::uintptr_t>(&Audio_Seq_unusedInventory);
    if (mode == "overlap-backward") {
      address -= sizeof(output) - 1;
    } else if (mode == "overlap-tail") {
      address += sizeof(Audio_Seq_unusedInventory) - 1;
    }
    target = reinterpret_cast<Items_EquipmentPartStats *>(address);
    expected = "equipment output overlaps inventory storage";
  } else if (mode == "invalid-stat" || mode == "changed-after-selection") {
    stat = -1;
    if (mode == "changed-after-selection") {
      partial = true;
      order[2] = 0;
      expected = identifier == 2583 || identifier == 2585
                     ? "part reference outside its inventory table"
                     : "sort source index outside inventory table";
    }
  }
  auto expected_records = std::bit_cast<RecordBytes>(Audio_Seq_unusedInventory);
  const auto before_order =
      std::bit_cast<IndexBytes>(Audio_Seq_unusedInventoryIndices);
  const auto dropped = VS::Render::DroppedLiveUiPacketCount();
  const auto *ordering_table = Render_legacyCurrentOrderingTable;
  const std::array primitive_buffers{Render_legacyPrimitiveBuffers[0],
                                     Render_legacyPrimitiveBuffers[1]};
  const auto packets_unchanged = [&] {
    return dropped == VS::Render::DroppedLiveUiPacketCount() &&
           ordering_table == Render_legacyCurrentOrderingTable &&
           primitive_buffers == std::array{Render_legacyPrimitiveBuffers[0],
                                           Render_legacyPrimitiveBuffers[1]};
  };
  static_cast<void>(VS::Core::SetLogObserver([&](const auto &event) {
    if (event.channel == "menu.framework") {
      ++framework_warnings;
      if (partial && framework_warnings == 3) {
        if (identifier == 2583) {
          std::span(Audio_Seq_unusedInventory.weapons).back().blade =
              items::kBladeSlots + 1;
        } else if (identifier == 2585) {
          std::span(Audio_Seq_unusedInventory.shields).back().gems[0] =
              kWideToken;
        } else {
          order[1] = kWideToken;
        }
        expected_records =
            std::bit_cast<RecordBytes>(Audio_Seq_unusedInventory);
      }
    }
    if (!expected.empty() &&
        event.message.find(expected) != std::string_view::npos) {
      bool preserved = std::bit_cast<RecordBytes>(Audio_Seq_unusedInventory) ==
                           expected_records &&
                       packets_unchanged();
      if (partial) {
        preserved = preserved && order[0] == 0 && order[2] == 0;
      } else {
        preserved =
            preserved && std::bit_cast<IndexBytes>(
                             Audio_Seq_unusedInventoryIndices) == before_order;
      }
      std::cout << "boundaries=" << (preserved ? "PASS" : "FAIL")
                << " packets=unchanged diagnosed=" << expected << '\n'
                << std::flush;
      static_cast<void>(VS::Core::FlushLogFile());
      std::_Exit(preserved ? kObservedRefusal : kFailedControl);
    }
  }));
  Invoke(identifier, category, stat, target, index);
  static_cast<void>(VS::Core::SetLogObserver({}));
  if (!expected.empty()) {
    std::cerr << "Expected refusal was not diagnosed\n";
    return kFailedControl;
  }
  bool passed = packets_unchanged();
  if (mode == "empty" || mode == "unsupported") {
    passed = passed && framework_warnings == 0 &&
             std::bit_cast<IndexBytes>(Audio_Seq_unusedInventoryIndices) ==
                 before_order;
  } else if (mode == "invalid-stat") {
    passed = passed && framework_warnings > 0;
  } else if (identifier == 2589) {
    passed = passed && output.id == kId && output.strength == kHigh;
  } else {
    const auto last = items::kCategoryCapacities[category];
    passed = passed && order[0] == last && order[1] == last && order[2] == 1;
    if (mode == "reset") {
      Boot_ResetMenu3Statics();
      passed = passed && order[0] == last && order[1] == last;
    }
  }
  std::cout << "boundaries=" << (passed ? "PASS" : "FAIL")
            << " packets=unchanged framework_warnings=" << framework_warnings
            << '\n';
  return passed ? 0 : kFailedControl;
}
