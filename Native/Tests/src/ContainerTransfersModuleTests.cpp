#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Items/ContainerIndexingLive.hpp"
#include "Items/ContainerTransfersLiveFixture.hpp"

extern "C" {
#include "Boot/OverlayReset.h"
}

namespace {
namespace items = VS::Items;
using container_transfers_tests::LiveFixture;

template <typename Entry>
bool IsBound(unsigned address, Entry entry) {
  const auto* resolved = VS::Content::ResolveModuleAddress(
      {Content_moduleSymbols, Content_moduleSymbolCount}, "MENU/MENUD.PRG",
      address);
  return resolved == reinterpret_cast<const void*>(entry);
}
}  // namespace

TEST_CASE("F02934 binds actual MENUD zero-argument preview entry",
          "[container-transfers-module][F02934]") {
  static_assert(
      std::is_same_v<decltype(&items::PreviewLiveTransferredMisc), void (*)()>);
  CHECK(IsBound(0x801031A0, &items::PreviewLiveTransferredMisc));
}

TEST_CASE("F02935 binds actual MENUD zero-argument refresh entry",
          "[container-transfers-module][F02935]") {
  static_assert(std::is_same_v<decltype(&items::RefreshLiveMiscTransferCount),
                               void (*)()>);
  CHECK(IsBound(0x80103270, &items::RefreshLiveMiscTransferCount));
}

TEST_CASE("F02936 binds actual MENUD four-argument int record-copy entry",
          "[container-transfers-module][F02936]") {
  static_assert(std::is_same_v<decltype(&items::CopyLiveContainerItem),
                               int (*)(int, Items_ContainerData*, int,
                                       Items_ContainerData*)>);
  CHECK(IsBound(0x801032AC, &items::CopyLiveContainerItem));
}

TEST_CASE("F02937 binds actual MENUD void category rebuild entry",
          "[container-transfers-module][F02937]") {
  static_assert(
      std::is_same_v<decltype(&items::RebuildLiveContainerCategoryIndices),
                     void (*)(int, Items_Container*)>);
  CHECK(IsBound(0x801037D8, &items::RebuildLiveContainerCategoryIndices));
}

TEST_CASE("F02938 binds actual MENUD zero-argument snapshot entry",
          "[container-transfers-module][F02938]") {
  CHECK(IsBound(0x801039AC, &items::SnapshotLiveGadgetBag));
}

TEST_CASE("F02939 binds actual MENUD zero-argument commit entry",
          "[container-transfers-module][F02939]") {
  CHECK(IsBound(0x80103B20, &items::CommitLiveGadgetBag));
}

TEST_CASE("F02940 binds actual MENUD three-argument int weapon parent entry",
          "[container-transfers-module][F02940]") {
  static_assert(std::is_same_v<decltype(&items::LiveContainerParentWeaponIndex),
                               int (*)(int, int, Items_ContainerData*)>);
  CHECK(IsBound(0x80103C9C, &items::LiveContainerParentWeaponIndex));
}

TEST_CASE("F02941 binds actual MENUD three-argument int shield parent entry",
          "[container-transfers-module][F02941]") {
  static_assert(std::is_same_v<decltype(&items::LiveContainerParentShieldIndex),
                               int (*)(int, int, Items_ContainerData*)>);
  CHECK(IsBound(0x80103D14, &items::LiveContainerParentShieldIndex));
}

TEST_CASE(
    "Ordinary MENUD reload retains workspace BSS and current transfer owners",
    "[container-transfers-module][F02934][F02935][F02936][F02937][F02938]["
    "F02939][F02940][F02941]") {
  LiveFixture fixture;
  auto& owner = fixture.storage->container;
  owner.items.misc[255].id = 15;
  owner.indices.misc[0] = 256;
  fixture.storage->itemsToTransfer[0] = 0x600;
  Menu_Container_transferredMiscCount = 91;
  Menu_Container_containerItemCapacities[6] = 0;
  Boot_ResetMenudStatics();
  CHECK(Menu_Framework_unusedInventoryStorage == fixture.storage.get());
  CHECK(Menu_Container_otherStorage == &owner);
  CHECK(Menu_Container_containerData == &owner);
  CHECK(Menu_Container_transferredMiscCount == 91);
  CHECK(fixture.storage->itemsToTransfer[0] == 0x600);
  CHECK(items::LiveContainerCategoryIndices(6, &owner) == owner.indices.misc);
  CHECK(items::CountLiveContainerItems(6, &owner.items) == 1);
}
