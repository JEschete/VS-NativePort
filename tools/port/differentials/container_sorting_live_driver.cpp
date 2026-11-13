#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include "Core/Log.hpp"
#include "Items/ContainerIndexingLiveFixture.hpp"
#include "Items/ContainerTables.hpp"
#include "Menu/Container/ContainerSortingLive.hpp"

extern "C" {
#include "Boot/OverlayReset.h"
}

namespace {
namespace sorting = VS::Menu::container;
namespace items = VS::Items;
using Fixture = container_indexing_tests::LiveFixture;
constexpr int kObservedRefusal = 85;
constexpr int kFailedControl = 86;
constexpr int kStrength = 8;
constexpr std::int8_t kLow = -128;
constexpr std::int8_t kHigh = 127;
constexpr std::uint8_t kId = 221;
constexpr std::uint16_t kWide = 65535;
constexpr std::int8_t kOpaque = 91;
using OwnerBytes = std::array<std::byte, sizeof(Items_Container)>;

std::span<std::uint16_t> Order(Items_Container &owner, int category) {
  switch (static_cast<items::ItemCategory>(category)) {
  case items::ItemCategory::kWeapon:
    return owner.indices.weapons;
  case items::ItemCategory::kBlade:
    return owner.indices.blades;
  case items::ItemCategory::kGrip:
    return owner.indices.grips;
  case items::ItemCategory::kShield:
    return owner.indices.shields;
  case items::ItemCategory::kArmor:
    return owner.indices.armor;
  case items::ItemCategory::kGem:
    return owner.indices.gems;
  case items::ItemCategory::kMisc:
    return owner.indices.misc;
  default:
    return {};
  }
}

void Parts(Items_Container &owner, int category) {
  items::Live::VisitContainerTable(
      owner.items, static_cast<items::ItemCategory>(category),
      [&](auto &table) {
        using Record = items::Live::TypedRecordOf<
            std::remove_reference_t<decltype(table[0])>>;
        if constexpr (requires(Record record) {
                        record.strength;
                        record.id;
                        record.index;
                      }) {
          auto first = items::Live::Typed(table[0]);
          first.id = kId;
          first.strength = kLow;
          items::Live::Commit(table[0], first);
          auto last = first;
          last.strength = kHigh;
          if constexpr (requires { last.classes; })
            last.classes.fill(kLow);
          items::Live::Commit(std::span(table).back(), last);
        }
      });
}

void Setup(Items_Container &owner, int identifier, int category) {
  auto order = Order(owner, category);
  order[0] = 1;
  order[1] = static_cast<std::uint16_t>(order.size());
  order[2] = order[1];
  if (identifier == 2961) {
    owner.items.weapons[0].blade = 1;
    std::span(owner.items.weapons).back().blade =
        static_cast<std::uint8_t>(std::size(owner.items.blades));
    owner.items.blades[0].strength = kLow;
    std::span(owner.items.blades).back().strength = kHigh;
  } else if (identifier == 2963) {
    owner.items.shields[0].base.strength = kLow;
    std::span(owner.items.shields).back().base.strength = kHigh;
    owner.items.shields[0].gems[0] = 1;
    std::span(owner.items.shields).back().gems[0] = 1;
  } else if (identifier == 2965) {
    owner.items.misc[0].id = 2;
    owner.items.misc[0].count = 1;
    std::span(owner.items.misc).back().id = 1;
    std::span(owner.items.misc).back().count = 255;
  } else {
    Parts(owner, category);
  }
}

void Invoke(int identifier, int category, int stat,
            Items_EquipmentPartStats *target, int index) {
  switch (identifier) {
  case 2961:
    sorting::SortLiveWeapons(stat);
    return;
  case 2963:
    sorting::SortLiveShields(stat);
    return;
  case 2965:
    sorting::SortLiveMisc(stat);
    return;
  case 2967:
    sorting::CopyLiveEquipmentStats(target, category, index);
    return;
  default:
    sorting::SortLiveEquipment(category, stat);
    return;
  }
}

bool Diagnostic(std::string_view message, std::string_view expected) {
  return message.find(expected) != std::string_view::npos;
}

std::byte *SourceAddress(Items_Container &owner, int category,
                         std::size_t index) {
  std::byte *result = nullptr;
  items::Live::VisitContainerTable(
      owner.items, static_cast<items::ItemCategory>(category),
      [&](auto &table) {
        result = reinterpret_cast<std::byte *>(&table[index]);
      });
  return result;
}
} // namespace

int main(int argc, char **argv) {
  if (argc < 3)
    return kFailedControl;
  const int identifier = std::atoi(argv[1]);
  const std::string mode = argv[2];
  int category = argc > 3             ? std::atoi(argv[3])
                 : identifier == 2961 ? 0
                 : identifier == 2963 ? 3
                 : identifier == 2965 ? 6
                                      : 1;
  Fixture fixture;
  Menu_Container_containerData = &fixture.owner;
  Setup(fixture.owner, identifier, category);
  Setup(fixture.replacement, identifier, category);
  Items_EquipmentPartStats output{};
  items::EquipmentPart initial{};
  initial.types.fill(kOpaque);
  initial.classes.fill(kOpaque);
  initial.affinities.fill(kOpaque);
  initial.current_dp = kWide;
  items::Live::Commit(output, initial);
  auto *target = &output;
  auto order = Order(fixture.owner, category);
  int index = static_cast<int>(order.size()) - 1;
  int stat = identifier == 2965 ? 1 : kStrength;
  std::string expected;
  bool partial = false;
  std::size_t warnings = 0;
  bool framework_warning = false;

  if (mode == "empty") {
    std::ranges::fill(order, 0);
    stat = std::numeric_limits<int>::min();
    if (identifier == 2967) {
      items::Live::VisitContainerTable(
          fixture.owner.items, static_cast<items::ItemCategory>(category),
          [&](auto &table) { table[index] = {}; });
    }
  } else if (mode == "ties") {
    items::Live::VisitContainerTable(
        fixture.owner.items, static_cast<items::ItemCategory>(category),
        [&](auto &table) { std::span(table).back() = table[0]; });
  } else if (mode == "current-owner") {
    Menu_Container_containerData = &fixture.replacement;
  } else if (mode == "missing") {
    Menu_Container_containerData = nullptr;
    expected = identifier == 2967 ? "missing container record owner"
                                  : "sort row outside captured container order";
  } else if (mode == "bounds" || mode == "wide-token") {
    if (identifier == 2967)
      index = mode == "wide-token" ? kWide : index + 1;
    else
      order[0] = mode == "wide-token" ? kWide : order.size() + 1;
    expected = "record index outside live container table";
  } else if (mode == "missing-output") {
    target = nullptr;
    expected = "missing equipment projection output";
  } else if (mode == "source-before-output") {
    target = nullptr;
    Menu_Container_containerData = nullptr;
    expected = "missing container record owner";
  } else if (mode == "negative-index") {
    index = -1;
    expected = "negative equipment projection source index";
  } else if (mode == "unsupported") {
    Menu_Container_containerData = nullptr;
    target = nullptr;
    category = 257;
    index = std::numeric_limits<int>::min();
  } else if (mode.starts_with("overlap")) {
    index = 0;
    auto *source = SourceAddress(fixture.owner, category, 0);
    const auto offset = mode == "overlap-forward"    ? 4
                        : mode == "overlap-backward" ? -4
                                                     : 0;
    target = reinterpret_cast<Items_EquipmentPartStats *>(source + offset);
    expected = "byte-overlapped equipment projection source and output";
  } else if (mode == "capacity-bounds") {
    std::ranges::fill(order, 0);
    Menu_Container_containerItemCapacities[category] =
        static_cast<std::uint16_t>(order.size() + 4);
    expected = "sort row outside captured container order";
  } else if (mode == "category-bounds") {
    category = -1;
    expected = "sort category outside container categories";
  } else if (mode == "category-bounds-high") {
    category = std::numeric_limits<int>::max();
    expected = "sort category outside container categories";
  } else if (mode == "zero-capacity") {
    Menu_Container_containerItemCapacities[category] = 0;
    Menu_Container_containerData = nullptr;
  } else if (mode == "missing-after-selection") {
    order[2] = 0;
    stat = -1;
    partial = true;
    expected = "missing container record owner";
  } else if (mode == "invalid-stat") {
    stat = -1;
  }
  const auto before = std::bit_cast<OwnerBytes>(fixture.owner);
  static_cast<void>(VS::Core::SetLogObserver([&](const auto &event) {
    if (event.channel == "menu.framework") {
      framework_warning = true;
      if (partial && ++warnings == 3)
        Menu_Container_containerData = nullptr;
    }
    if (!expected.empty() && Diagnostic(event.message, expected)) {
      bool preserved = std::bit_cast<OwnerBytes>(fixture.owner) == before;
      if (partial)
        preserved = order[0] == 0 && order[1] == order.size() && order[2] == 0;
      std::cerr << "boundaries=" << (preserved ? "PASS" : "FAIL")
                << " diagnosed=" << expected << '\n'
                << std::flush;
      std::_Exit(preserved ? kObservedRefusal : kFailedControl);
    }
  }));
  Invoke(identifier, category, stat, target, index);
  static_cast<void>(VS::Core::SetLogObserver({}));
  if (!expected.empty()) {
    std::cerr << "Expected refusal was not diagnosed\n";
    return kFailedControl;
  }
  bool passed = true;
  if (mode == "invalid-stat")
    passed = framework_warning;
  else if (mode == "empty" || mode == "unsupported" ||
           mode == "zero-capacity") {
    passed = !framework_warning &&
             std::bit_cast<OwnerBytes>(fixture.owner) == before;
    if (identifier == 2967 && mode == "empty") {
      passed = passed && output.id == 0 && output.strength == 0 &&
               items::Live::Typed(output).classes.back() == kOpaque;
    }
    if (mode == "unsupported") {
      passed = passed && items::Live::Typed(output) == initial;
    }
  } else if (identifier == 2967) {
    const auto typed = items::Live::Typed(output);
    passed = typed.id == kId && typed.strength == kHigh &&
             typed.classes.back() == kOpaque &&
             typed.affinities.back() == kOpaque;
  } else {
    const auto actual =
        Order(mode == "current-owner" ? fixture.replacement : fixture.owner,
              category);
    passed = mode == "ties" ? actual[0] == 1 && actual[1] == actual.size() &&
                                  actual[2] == actual.size()
                            : actual[0] == actual.size() &&
                                  actual[1] == actual.size() && actual[2] == 1;
    if (mode == "reset") {
      const auto *owner = Menu_Container_containerData;
      Boot_ResetMenudStatics();
      passed = passed && Menu_Container_containerData == owner &&
               actual[0] == actual.size() && actual[1] == actual.size();
    }
  }
  std::cout << "boundaries=" << (passed ? "PASS" : "FAIL") << '\n';
  return passed ? 0 : kFailedControl;
}
