#include <array>
#include <cstddef>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Menu/Inventory/InventorySortingLive.hpp"

namespace {
namespace sorting = VS::Menu::Inventory;
using SortEntry = void (*)(int);
constexpr std::array<SortEntry, 3> kSortEntries = {
    sorting::SortLiveWeapons,
    sorting::SortLiveShields,
    sorting::SortLiveMisc,
};
constexpr std::array<unsigned, 3> kSortAddresses = {
    0x80103CC8,
    0x80103F00,
    0x8010408C,
};
constexpr unsigned kProjection = 0x801042C4;
constexpr unsigned kEquipment = 0x8010439C;
constexpr unsigned kMenubOffset = 0xD8;
}  // namespace

TEST_CASE("Inventory registry retains all five actual MENU3 signatures",
          "[inventory-sorting-module]") {
  static_assert(std::is_same_v<decltype(&sorting::CopyLiveEquipmentStats),
                               void (*)(Items_EquipmentPartStats*, int, int)>);
  static_assert(std::is_same_v<decltype(&sorting::SortLiveEquipment),
                               void (*)(int, int)>);
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  for (std::size_t row = 0; row < kSortEntries.size(); ++row) {
    CHECK(VS::Content::ResolveModuleAddress(symbols, "MENU/MENU3.PRG",
                                            kSortAddresses[row]) ==
          reinterpret_cast<const void*>(kSortEntries[row]));
    CHECK(VS::Content::ResolveModuleAddress(
              symbols, "MENU/MENUB.PRG", kSortAddresses[row] + kMenubOffset) ==
          reinterpret_cast<const void*>(kSortEntries[row]));
  }
  CHECK(VS::Content::ResolveModuleAddress(symbols, "MENU/MENU3.PRG",
                                          kProjection) ==
        reinterpret_cast<const void*>(&sorting::CopyLiveEquipmentStats));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "MENU/MENU3.PRG",
                                          kEquipment) ==
        reinterpret_cast<const void*>(&sorting::SortLiveEquipment));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "MENU/MENUB.PRG",
                                          kProjection + kMenubOffset) ==
        reinterpret_cast<const void*>(&sorting::CopyLiveEquipmentStats));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "MENU/MENUB.PRG",
                                          kEquipment + kMenubOffset) ==
        reinterpret_cast<const void*>(&sorting::SortLiveEquipment));
}
