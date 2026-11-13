#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <string_view>

#include "Items/ContainerIndexingLiveFixture.hpp"

extern "C" {
unsigned short Menu_Container_containerItemCapacities[8]{32, 64,  64,  32,
                                                         64, 192, 256, 2};
unsigned short Menu_Container_transferredMiscCount{};
Items_Container *Menu_Container_containerData{};
}

namespace {
namespace items = VS::Items;
using container_indexing_tests::LiveFixture;
std::string_view expected;
std::function<bool()> diagnostic_state;

void Invoke(int id, int category, int index, LiveFixture &fixture,
            bool missing = false) {
  auto *owner = missing ? nullptr : &fixture.owner;
  auto *records = owner == nullptr ? nullptr : &owner->items;
  auto *order = owner == nullptr ? nullptr : owner->indices.misc;
  if (category == 0 && owner != nullptr)
    order = owner->indices.weapons;
  switch (id) {
  case 2927:
    static_cast<void>(items::LiveContainerCategoryIndices(category, owner));
    break;
  case 2928:
    static_cast<void>(items::FindLiveContainerItemRow(category, index, order));
    break;
  case 2929:
    items::AppendLiveContainerItemIndex(category, index, order);
    break;
  case 2930:
    items::ClearLiveContainerItem(category, index, records);
    break;
  case 2931:
    static_cast<void>(items::LiveContainerItemId(category, index, records));
    break;
  case 2932:
    static_cast<void>(items::CountLiveContainerItems(category, records));
    break;
  case 2933:
    static_cast<void>(items::FindLiveEmptyContainerSlot(category, records));
    break;
  default:
    std::exit(86);
  }
}

bool CurrentOwner(int id, LiveFixture &fixture) {
  auto &a = fixture.owner;
  auto &b = fixture.replacement;
  Menu_Container_containerData = &b;
  a.items.misc[0].id = 65535;
  b.items.misc[1].id = 32768;
  a.indices.misc[0] = 256;
  b.indices.misc[1] = 1;
  switch (id) {
  case 2927:
    return items::LiveContainerCategoryIndices(6, &a) == a.indices.misc &&
           items::LiveContainerCategoryIndices(6, &b) == b.indices.misc;
  case 2928:
    return items::FindLiveContainerItemRow(6, 255, a.indices.misc) == 1 &&
           items::FindLiveContainerItemRow(6, 255, b.indices.misc) == 0;
  case 2929:
    items::AppendLiveContainerItemIndex(6, 65534, a.indices.misc);
    return a.indices.misc[1] == 65535 && b.indices.misc[0] == 0 &&
           b.indices.misc[1] == 1;
  case 2930:
    items::ClearLiveContainerItem(6, 0, &a.items);
    return a.items.misc[0].id == 0 && b.items.misc[1].id == 32768;
  case 2931:
    return items::LiveContainerItemId(6, 0, &a.items) == 65535 &&
           items::LiveContainerItemId(6, 0, &b.items) == 0 &&
           items::LiveContainerItemId(6, 1, &b.items) == 32768;
  case 2932:
    b.items.misc[2].id = 1;
    return items::CountLiveContainerItems(6, &a.items) == 1 &&
           items::CountLiveContainerItems(6, &b.items) == 2;
  case 2933:
    return items::FindLiveEmptyContainerSlot(6, &a.items) == 1 &&
           items::FindLiveEmptyContainerSlot(6, &b.items) == 0;
  default:
    return false;
  }
}

bool Lazy(int id, LiveFixture &fixture) {
  if (id == 2927) {
    std::fill_n(Menu_Container_containerItemCapacities, 8, 65535);
    return items::LiveContainerCategoryIndices(0, &fixture.owner) ==
           fixture.owner.indices.weapons;
  }
  if (id == 2930 || id == 2931) {
    std::fill_n(Menu_Container_containerItemCapacities, 8, 65535);
    Invoke(id, 7, std::numeric_limits<int>::max(), fixture, true);
    return true;
  }
  Menu_Container_containerItemCapacities[6] = 0;
  Invoke(id, 6, 0, fixture, true);
  return true;
}

bool Extra(int id, std::string_view mode, LiveFixture &fixture) {
  if (mode == "manage") {
    Invoke(id, 7, -1, fixture, id >= 2930);
    return true;
  }
  if (mode == "empty-record") {
    if (id == 2930) {
      std::fill_n(Menu_Container_containerItemCapacities, 8, 65535);
      items::ClearLiveContainerItem(1, 0, &fixture.owner.items);
      return items::Live::Typed(fixture.owner.items.blades[0]) ==
             items::InventoryBlade{.index = 1};
    }
    return items::LiveContainerItemId(6, 255, &fixture.owner.items) == 0;
  }
  if (mode == "full") {
    if (id == 2933 || id == 2932) {
      for (auto &item : fixture.owner.items.misc)
        item.id = 65535;
      return (id == 2932
                  ? items::CountLiveContainerItems(6, &fixture.owner.items)
                  : items::FindLiveEmptyContainerSlot(
                        6, &fixture.owner.items)) == 256;
    }
    std::fill_n(fixture.owner.indices.misc, 256, 65535);
    if (id == 2928)
      return items::FindLiveContainerItemRow(6, 65535,
                                             fixture.owner.indices.misc) == 0;
    items::AppendLiveContainerItemIndex(6, 1, fixture.owner.indices.misc);
    return std::all_of(std::begin(fixture.owner.indices.misc),
                       std::end(fixture.owner.indices.misc),
                       [](unsigned short value) { return value == 65535; });
  }
  return false;
}
} // namespace

extern "C" void Core_Errorf(const char *channel, const char *format, ...) {
  char text[1024]{};
  va_list arguments;
  va_start(arguments, format);
  std::vsnprintf(text, sizeof(text), format, arguments);
  va_end(arguments);
  const bool valid =
      std::string_view(channel) == "items.container" && !expected.empty() &&
      std::string_view(text).find(expected) != std::string_view::npos &&
      (!diagnostic_state || diagnostic_state());
  std::fprintf(stderr, "%s boundaries=%s\n", text, valid ? "PASS" : "FAIL");
  std::exit(valid ? 85 : 86);
}

int main(int argc, char **argv) {
  if (argc != 3)
    return 86;
  const int id = std::atoi(argv[1]);
  const std::string_view mode = argv[2];
  LiveFixture fixture;
  bool valid = false;
  if (mode == "bad-category") {
    expected = "category outside container categories";
    Invoke(id, -1, 0, fixture, true);
  } else if (mode == "missing") {
    expected = id == 2927   ? "missing container index owner"
               : id <= 2929 ? "missing or short container display order"
                            : "missing container record owner";
    Invoke(id, 6, 0, fixture, true);
  } else if (mode == "bad-capacity") {
    expected = "capacity exceeds container category storage";
    const int category = id == 2927 ? 0 : 6;
    Menu_Container_containerItemCapacities[category] =
        items::kContainerCapacities[static_cast<std::size_t>(category)] + 1;
    Invoke(id, id == 2927 ? 1 : 6, 0, fixture, true);
  } else if (mode == "low-index" || mode == "high-index") {
    expected = "record index outside container category storage";
    Invoke(id, 6, mode == "low-index" ? -1 : 256, fixture, true);
  } else if (mode == "zero-blade") {
    expected = "record index outside container category storage";
    fixture.owner.items.weapons[0].grip = 1;
    fixture.owner.items.shields[31].base.id = 9;
    diagnostic_state = [&] {
      return fixture.owner.items.shields[31].base.id == 9 &&
             fixture.owner.items.weapons[0].grip == 1;
    };
    Invoke(id, 0, 0, fixture);
  } else if (mode == "zero-grip") {
    expected = "record index outside container category storage";
    fixture.owner.items.weapons[0].blade = 1;
    fixture.owner.items.blades[0].id = 7;
    diagnostic_state = [&] {
      return items::Live::Typed(fixture.owner.items.blades[0]) ==
                 items::InventoryBlade{.index = 1} &&
             fixture.owner.items.weapons[0].blade == 1;
    };
    Invoke(id, 0, 0, fixture);
  } else if (mode == "bad-gem") {
    expected = "record index outside container category storage";
    fixture.owner.items.shields[0].gems[0] = 193;
    Invoke(id, 3, 0, fixture);
  } else if (mode == "lazy") {
    valid = Lazy(id, fixture);
  } else if (mode == "current-owner") {
    valid = CurrentOwner(id, fixture);
  } else {
    valid = Extra(id, mode, fixture);
  }
  std::printf("boundaries=%s\n", valid && expected.empty() ? "PASS" : "FAIL");
  return valid && expected.empty() ? 0 : 86;
}
