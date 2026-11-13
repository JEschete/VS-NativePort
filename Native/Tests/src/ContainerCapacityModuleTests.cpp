#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Items/ContainerGemCountsLive.hpp"
#include "Items/ContainerTransfersLiveFixture.hpp"
#include "Menu/Container/ContainerCapacityLive.hpp"

extern "C" {
#include "Boot/OverlayReset.h"
}

namespace {
namespace items = VS::Items;
namespace capacity = VS::Menu::container;

template <typename Entry>
bool IsBound(unsigned address, Entry entry) {
  return VS::Content::ResolveModuleAddress(
             {Content_moduleSymbols, Content_moduleSymbolCount},
             "MENU/MENUD.PRG", address) == reinterpret_cast<const void*>(entry);
}
}  // namespace

TEST_CASE("F02971 binds the actual three-integer void draw entry",
          "[container-capacity-module][F02971]") {
  static_assert(std::is_same_v<decltype(&capacity::DrawLiveCapacityCount),
                               void (*)(int, int, int)>);
  CHECK(IsBound(0x80106464, &capacity::DrawLiveCapacityCount));
}

TEST_CASE("F02972 binds the actual no-argument void summaries entry",
          "[container-capacity-module][F02972]") {
  static_assert(std::is_same_v<decltype(&capacity::DrawLiveStorageCapacities),
                               void (*)()>);
  CHECK(IsBound(0x80106504, &capacity::DrawLiveStorageCapacities));
}

TEST_CASE("F02973 binds the actual integer weapon gem count entry",
          "[container-capacity-module][F02973]") {
  static_assert(std::is_same_v<decltype(&items::CountLiveContainerWeaponGems),
                               int (*)(int)>);
  CHECK(IsBound(0x80106624, &items::CountLiveContainerWeaponGems));
}

TEST_CASE("F02974 binds the actual integer shield gem count entry",
          "[container-capacity-module][F02974]") {
  static_assert(std::is_same_v<decltype(&items::CountLiveContainerShieldGems),
                               int (*)(int)>);
  CHECK(IsBound(0x80106684, &items::CountLiveContainerShieldGems));
}

TEST_CASE("F02975 binds the actual two-integer receiving capacity entry",
          "[container-capacity-module][F02975]") {
  static_assert(std::is_same_v<decltype(&capacity::WouldExceedLiveCapacity),
                               int (*)(int, int)>);
  CHECK(IsBound(0x801066DC, &capacity::WouldExceedLiveCapacity));
}

TEST_CASE("F02976 binds the actual two-integer error-code entry",
          "[container-capacity-module][F02976]") {
  static_assert(std::is_same_v<decltype(&capacity::LiveTransferCapacityError),
                               int (*)(int, int)>);
  CHECK(IsBound(0x80106784, &capacity::LiveTransferCapacityError));
}

TEST_CASE("F02977 binds the actual integer-row void undo entry",
          "[container-capacity-module][F02977]") {
  static_assert(std::is_same_v<decltype(&capacity::UndoLivePendingTransfer),
                               void (*)(int)>);
  CHECK(IsBound(0x801068BC, &capacity::UndoLivePendingTransfer));
}

TEST_CASE("F02978 binds the actual two-integer void staging entry",
          "[container-capacity-module][F02978]") {
  static_assert(std::is_same_v<decltype(&capacity::StageLiveItemTransfer),
                               void (*)(int, int)>);
  CHECK(IsBound(0x80106948, &capacity::StageLiveItemTransfer));
}
