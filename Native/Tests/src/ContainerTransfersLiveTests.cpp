#include <catch2/catch_test_macros.hpp>

#include "Items/ContainerIndexing.hpp"
#include "Items/ContainerTables.hpp"
#include "Items/ContainerTransfersLiveFixture.hpp"

namespace {
namespace items = VS::Items;
using container_transfers_tests::LiveFixture;
}  // namespace

TEST_CASE(
    "F02934 Live uses current workspace and actual selected other storage",
    "[container-transfers-live][F02934]") {
  LiveFixture fixture;
  fixture.storage->container.items.misc[0] = {12, 99, 71};
  fixture.storage->stagedTransfer.misc[255] = {12, 2, 91};
  fixture.storage->itemsToTransfer[0] = 0x600;
  items::PreviewLiveTransferredMisc();
  CHECK(fixture.storage->transferPreview.misc[0].count == 100);
  CHECK(fixture.storage->transferPreview.misc[1].count == 1);
  Menu_Framework_unusedInventoryStorage = fixture.replacement.get();
  Menu_Container_otherStorage = &fixture.replacement->gadgetBag;
  fixture.replacement->gadgetBag.items.misc[2].id = 15;
  items::PreviewLiveTransferredMisc();
  CHECK(fixture.replacement->transferPreview.misc[2].id == 15);
  CHECK(fixture.storage->transferPreview.misc[0].id == 12);
}

TEST_CASE(
    "F02935 Live refresh updates count only after rebuilding current preview",
    "[container-transfers-live][F02935]") {
  LiveFixture fixture;
  fixture.storage->container.items.misc[255].id = 65535;
  Menu_Container_transferredMiscCount = 99;
  items::RefreshLiveMiscTransferCount();
  CHECK(Menu_Container_transferredMiscCount == 1);
  Menu_Framework_unusedInventoryStorage = fixture.replacement.get();
  Menu_Container_otherStorage = &fixture.replacement->container;
  items::RefreshLiveMiscTransferCount();
  CHECK(Menu_Container_transferredMiscCount == 0);
  CHECK(fixture.storage->transferPreview.misc[255].id == 65535);
}

TEST_CASE("F02936 Live copies real typed records and retains source aliases",
          "[container-transfers-live][F02936]") {
  LiveFixture fixture;
  auto& owner = fixture.storage->container.items;
  owner.misc[0] = {2, 60, 77};
  CHECK(items::CopyLiveContainerItem(0x16, &owner, 0, &owner) == 2);
  CHECK(owner.misc[0].count == 100);
  CHECK(owner.misc[1].count == 20);
  CHECK(owner.misc[1].index == 0);
  CHECK(items::CopyLiveContainerItem(1, &owner, -1, nullptr) == 1);
}

TEST_CASE(
    "F02936 Live copies exact empty record metadata through existing Typed "
    "Commit",
    "[container-transfers-live][F02936]") {
  LiveFixture fixture;
  items::InventoryBlade source{};
  source.opaque12 = 65535;
  source.type_bits = 255;
  items::Live::Commit(fixture.storage->stagedTransfer.blades[63], source);
  CHECK(items::CopyLiveContainerItem(0xFF11, &fixture.storage->container.items,
                                     63,
                                     &fixture.storage->stagedTransfer) == 1);
  auto expected = source;
  expected.index = 1;
  expected.assembled_weapon_index = 255;
  CHECK(items::Live::Typed(fixture.storage->container.items.blades[0]) ==
        expected);
}

TEST_CASE(
    "F02937 Live rebuild delegates to original validated indexing policies",
    "[container-transfers-live][F02937]") {
  LiveFixture fixture;
  auto& owner = fixture.storage->container;
  owner.items.misc[255].id = 65535;
  owner.items.misc[0].id = 1;
  owner.indices.misc[0] = 256;
  owner.indices.misc[2] = 256;
  owner.indices.misc[3] = 2;
  Menu_Container_containerData = &fixture.replacement->container;
  items::RebuildLiveContainerCategoryIndices(6, &owner);
  CHECK(owner.indices.misc[0] == 256);
  CHECK(owner.indices.misc[1] == 256);
  CHECK(owner.indices.misc[2] == 1);
  CHECK(owner.indices.misc[3] == 0);
  CHECK(fixture.replacement->container.indices.misc[0] == 0);
}

TEST_CASE(
    "F02938 Live snapshot preserves inventory quantities and zero extends byte "
    "rows",
    "[container-transfers-live][F02938]") {
  LiveFixture fixture;
  Audio_Seq_unusedInventory.misc[63] = {65535, static_cast<char>(255), 91};
  fixture.order.misc[0] = 255;
  fixture.storage->gadgetBag.items.misc[255].id = 99;
  items::SnapshotLiveGadgetBag();
  CHECK(fixture.storage->gadgetBag.items.misc[63].id == 65535);
  CHECK(fixture.storage->gadgetBag.items.misc[63].count ==
        static_cast<char>(255));
  CHECK(fixture.storage->gadgetBag.items.misc[255].id == 0);
  CHECK(fixture.storage->gadgetBag.indices.misc[0] == 255);
  CHECK(fixture.storage->gadgetBag.indices.misc[64] == 0);
}

TEST_CASE("F02939 Live commits low token bytes without remapping item records",
          "[container-transfers-live][F02939]") {
  LiveFixture fixture;
  auto& bag = fixture.storage->gadgetBag;
  bag.items.misc[0] = {11, 99, 91};
  bag.items.misc[255].id = 15;
  bag.indices.misc[0] = 256;
  bag.indices.misc[1] = 1;
  items::CommitLiveGadgetBag();
  CHECK(fixture.order.misc[0] == 0);
  CHECK(fixture.order.misc[1] == 1);
  CHECK(Audio_Seq_unusedInventory.misc[0].id == 11);
  CHECK(Audio_Seq_unusedInventory.misc[0].index == 91);
  CHECK(bag.items.misc[255].id == 15);
}

TEST_CASE(
    "F02940 Live returns actual owner part widths and ignores unrelated "
    "categories lazily",
    "[container-transfers-live][F02940]") {
  LiveFixture fixture;
  fixture.storage->container.items.grips[63].assembledWeaponIndex = 65535;
  fixture.replacement->container.items.grips[63].assembledWeaponIndex = 2;
  CHECK(items::LiveContainerParentWeaponIndex(
            2, 63, &fixture.storage->container.items) == 65535);
  CHECK(items::LiveContainerParentWeaponIndex(
            2, 63, &fixture.replacement->container.items) == 2);
  CHECK(items::LiveContainerParentWeaponIndex(257, -1, nullptr) == 0);
}

TEST_CASE(
    "F02941 Live excludes weapon gems and masks only shield parent result",
    "[container-transfers-live][F02941]") {
  LiveFixture fixture;
  auto& owner = fixture.storage->container.items;
  owner.gems[191].setItemIndex = 0xFF81;
  CHECK(items::LiveContainerParentShieldIndex(5, 191, &owner) == 1);
  owner.gems[191].setItemIndex = 0xFF01;
  CHECK(items::LiveContainerParentShieldIndex(5, 191, &owner) == 0);
  CHECK(items::LiveContainerParentShieldIndex(261, -1, nullptr) == 0);
}
