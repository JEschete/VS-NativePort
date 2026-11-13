#include <array>
#include <cstddef>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Items/ContainerIndexingLiveFixture.hpp"
#include "Menu/Container/ContainerSortingLive.hpp"

extern "C" {
#include "Boot/OverlayReset.h"
}

namespace {
namespace sorting = VS::Menu::container;
using container_indexing_tests::LiveFixture;
using SortEntry = void (*)(int);
constexpr std::array<SortEntry, 3> kSortEntries = {
    sorting::SortLiveWeapons,
    sorting::SortLiveShields,
    sorting::SortLiveMisc,
};
constexpr std::array<unsigned, 3> kSortAddresses = {
    0x801051F8,
    0x80105454,
    0x801055F0,
};
constexpr unsigned kProjectionAddress = 0x80105844;
constexpr unsigned kEquipmentAddress = 0x8010592C;
constexpr int kGem = static_cast<int>(VS::Items::ItemCategory::kGem);
constexpr int kStrength = 8;
constexpr std::int8_t kPositive = 127;
constexpr std::int8_t kNegative = -128;
}  // namespace

TEST_CASE("Container sorting registry uses all five actual MENUD signatures",
          "[container-sorting-module]") {
  static_assert(std::is_same_v<decltype(&sorting::CopyLiveEquipmentStats),
                               void (*)(Items_EquipmentPartStats*, int, int)>);
  static_assert(std::is_same_v<decltype(&sorting::SortLiveEquipment),
                               void (*)(int, int)>);
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  for (std::size_t row = 0; row < kSortEntries.size(); ++row) {
    CHECK(VS::Content::ResolveModuleAddress(symbols, "MENU/MENUD.PRG",
                                            kSortAddresses[row]) ==
          reinterpret_cast<const void*>(kSortEntries[row]));
  }
  CHECK(VS::Content::ResolveModuleAddress(symbols, "MENU/MENUD.PRG",
                                          kProjectionAddress) ==
        reinterpret_cast<const void*>(&sorting::CopyLiveEquipmentStats));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "MENU/MENUD.PRG",
                                          kEquipmentAddress) ==
        reinterpret_cast<const void*>(&sorting::SortLiveEquipment));
}

TEST_CASE(
    "Ordinary MENUD reload retains sorted tokens and current record owner",
    "[container-sorting-module][container-sorting-reload][F02968]") {
  LiveFixture fixture;
  Menu_Container_containerData = &fixture.replacement;
  auto& owner = fixture.replacement;
  constexpr auto kLast = VS::Items::kContainerCapacities[kGem];
  owner.items.gems[0].strength = kNegative;
  std::span(owner.items.gems).back().strength = kPositive;
  owner.indices.gems[0] = 1;
  owner.indices.gems[1] = kLast;
  owner.indices.gems[2] = kLast;
  sorting::SortLiveEquipment(kGem, kStrength);
  Boot_ResetMenudStatics();
  CHECK(Menu_Container_containerData == &owner);
  CHECK(owner.indices.gems[0] == kLast);
  CHECK(owner.indices.gems[1] == kLast);
  CHECK(owner.indices.gems[2] == 1);
  CHECK(Menu_Container_containerItemCapacities[kGem] == kLast);
  owner.items.gems[0].strength = kPositive;
  std::span(owner.items.gems).back().strength = kNegative;
  sorting::SortLiveEquipment(kGem, kStrength);
  CHECK(owner.indices.gems[0] == 1);
  CHECK(owner.indices.gems[1] == kLast);
}
