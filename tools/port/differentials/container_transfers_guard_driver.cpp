#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string_view>

#include "Items/ContainerIndexing.hpp"
#include "Items/ContainerTransfersLiveFixture.hpp"

extern "C" {
unsigned short Menu_Container_containerItemCapacities[8]{32, 64,  64,  32,
                                                         64, 192, 256, 2};
unsigned short Menu_Container_transferredMiscCount{};
Items_Container *Menu_Container_containerData{};
Items_Container *Menu_Container_otherStorage{};
Menu_Container_InventoryStorage *Menu_Framework_unusedInventoryStorage{};
Items_Inventory Audio_Seq_unusedInventory{};
unsigned char *Menu_Inventory_inventoryIndices[8]{};
extern const unsigned char Items_Inventory_inventoryItemCapacities[8]{
    8, 16, 16, 8, 16, 48, 64, 2};
}

namespace {
namespace items = VS::Items;
using container_transfers_tests::LiveFixture;
std::string_view expected;
std::function<bool()> diagnostic_state;

bool CurrentOwner(int id, LiveFixture &fixture) {
  auto &first = *fixture.storage;
  auto &second = *fixture.replacement;
  switch (id) {
  case 2934:
  case 2935:
    first.container.items.misc[0] = {12, 99, 71};
    first.stagedTransfer.misc[255] = {12, 2, 91};
    first.itemsToTransfer[0] = 0x600;
    if (id == 2934)
      items::PreviewLiveTransferredMisc();
    else
      items::RefreshLiveMiscTransferCount();
    if (first.transferPreview.misc[1].count != 1)
      return false;
    Menu_Framework_unusedInventoryStorage = &second;
    Menu_Container_otherStorage = &second.gadgetBag;
    second.gadgetBag.items.misc[255].id = 15;
    if (id == 2934)
      items::PreviewLiveTransferredMisc();
    else
      items::RefreshLiveMiscTransferCount();
    return second.transferPreview.misc[255].id == 15 &&
           first.transferPreview.misc[0].id == 12 &&
           (id != 2935 || Menu_Container_transferredMiscCount == 1);
  case 2936:
    first.stagedTransfer.blades[63].id = 255;
    Menu_Container_containerData = &second.container;
    return items::CopyLiveContainerItem(0x8111, &first.container.items, 63,
                                        &first.stagedTransfer) == 1 &&
           first.container.items.blades[0].id == 255 &&
           static_cast<unsigned char>(
               first.container.items.blades[0].assembledWeaponIndex) == 0x81 &&
           second.container.items.blades[0].id == 0;
  case 2937:
    first.container.items.misc[255].id = 12;
    second.container.items.misc[1].id = 13;
    items::RebuildLiveContainerCategoryIndices(6, &first.container);
    items::RebuildLiveContainerCategoryIndices(6, &second.container);
    return first.container.indices.misc[0] == 256 &&
           second.container.indices.misc[0] == 2;
  case 2938:
    Audio_Seq_unusedInventory.misc[63] = {65535, 99, 71};
    fixture.order.misc[0] = 255;
    items::SnapshotLiveGadgetBag();
    Menu_Framework_unusedInventoryStorage = &second;
    Audio_Seq_unusedInventory.misc[63].id = 15;
    items::SnapshotLiveGadgetBag();
    return first.gadgetBag.items.misc[63].id == 65535 &&
           second.gadgetBag.items.misc[63].id == 15 &&
           second.gadgetBag.indices.misc[0] == 255;
  case 2939:
    first.gadgetBag.items.misc[63].id = 12;
    items::CommitLiveGadgetBag();
    if (Audio_Seq_unusedInventory.misc[63].id != 12)
      return false;
    Menu_Framework_unusedInventoryStorage = &second;
    second.gadgetBag.items.misc[63].id = 15;
    items::CommitLiveGadgetBag();
    return Audio_Seq_unusedInventory.misc[63].id == 15 &&
           first.gadgetBag.items.misc[63].id == 12;
  case 2940:
    first.container.items.grips[63].assembledWeaponIndex = 65535;
    second.container.items.grips[63].assembledWeaponIndex = 2;
    return items::LiveContainerParentWeaponIndex(
               2, 63, &first.container.items) == 65535 &&
           items::LiveContainerParentWeaponIndex(2, 63,
                                                 &second.container.items) == 2;
  case 2941:
    first.container.items.gems[191].setItemIndex = 0xFF81;
    second.container.items.gems[191].setItemIndex = 0xFF01;
    return items::LiveContainerParentShieldIndex(5, 191,
                                                 &first.container.items) == 1 &&
           items::LiveContainerParentShieldIndex(5, 191,
                                                 &second.container.items) == 0;
  default:
    return false;
  }
}

bool Lazy(int id, LiveFixture &fixture) {
  auto &owner = *fixture.storage;
  switch (id) {
  case 2934:
  case 2935:
    owner.container.items.misc[255].id = 15;
    owner.itemsToTransfer[0] = 0x101;
    owner.itemsToTransfer[1] = 0;
    owner.stagedTransfer.weapons[0].blade = 0;
    if (id == 2934)
      items::PreviewLiveTransferredMisc();
    else
      items::RefreshLiveMiscTransferCount();
    return owner.transferPreview.misc[255].id == 15;
  case 2936:
    return items::CopyLiveContainerItem(1, &owner.container.items, -1,
                                        nullptr) == 1 &&
           items::CopyLiveContainerItem(7, nullptr, -1, nullptr) == 0;
  case 2937:
    owner.container.items.weapons[0].grip = 193;
    items::RebuildLiveContainerCategoryIndices(1, &owner.container);
    return owner.container.indices.blades[0] == 0;
  case 2938:
    owner.gadgetBag.items.weapons[0].grip = 193;
    Audio_Seq_unusedInventory.weapons[0].grip = 0;
    items::SnapshotLiveGadgetBag();
    return owner.gadgetBag.items.weapons[0].grip == 0;
  case 2939:
    owner.gadgetBag.items.weapons[0].blade = 0;
    owner.gadgetBag.items.weapons[0].grip = 193;
    items::CommitLiveGadgetBag();
    return Audio_Seq_unusedInventory.weapons[0].grip == 193;
  case 2940:
    return items::LiveContainerParentWeaponIndex(257, -1, nullptr) == 0;
  case 2941:
    return items::LiveContainerParentShieldIndex(261, -1, nullptr) == 0;
  default:
    return false;
  }
}

bool Alias(int id, LiveFixture &fixture) {
  auto &owner = *fixture.storage;
  switch (id) {
  case 2934:
  case 2935:
    owner.stagedTransfer.misc[0] = {12, 60, 99};
    owner.itemsToTransfer[0] = 0x601;
    owner.itemsToTransfer[1] = 0x601;
    if (id == 2934)
      items::PreviewLiveTransferredMisc();
    else
      items::RefreshLiveMiscTransferCount();
    return owner.transferPreview.misc[0].count == 100 &&
           owner.transferPreview.misc[1].count == 20 &&
           owner.stagedTransfer.misc[0].count == 60 &&
           (id != 2935 || Menu_Container_transferredMiscCount == 2);
  case 2936:
    owner.container.items.misc[0] = {12, 60, 77};
    return items::CopyLiveContainerItem(0x16, &owner.container.items, 0,
                                        &owner.container.items) == 2 &&
           owner.container.items.misc[0].count == 100 &&
           owner.container.items.misc[1].count == 20;
  case 2937:
    owner.container.items.blades[0].id = 1;
    owner.container.indices.blades[0] = 1;
    owner.container.indices.blades[2] = 1;
    items::RebuildLiveContainerCategoryIndices(1, &owner.container);
    return owner.container.indices.blades[0] == 1 &&
           owner.container.indices.blades[1] == 1;
  case 2938:
    fixture.order.blades[0] = 255;
    Menu_Inventory_inventoryIndices[2] = fixture.order.blades;
    items::SnapshotLiveGadgetBag();
    return owner.gadgetBag.indices.blades[0] == 255 &&
           owner.gadgetBag.indices.grips[0] == 255;
  case 2939:
    owner.gadgetBag.items.blades[0].id = 1;
    owner.gadgetBag.items.grips[1].id = 2;
    Menu_Inventory_inventoryIndices[2] = fixture.order.blades;
    items::CommitLiveGadgetBag();
    return fixture.order.blades[0] == 2 && fixture.order.grips[0] == 0;
  case 2940:
    owner.container.items.gems[0].setItemIndex = 0x101;
    owner.container.items.gems[1].setItemIndex = 0x101;
    return items::LiveContainerParentWeaponIndex(
               5, 0, &owner.container.items) == 0x101 &&
           items::LiveContainerParentWeaponIndex(
               5, 1, &owner.container.items) == 0x101;
  case 2941:
    owner.container.items.gems[0].setItemIndex = 0x181;
    owner.container.items.gems[1].setItemIndex = 0x181;
    return items::LiveContainerParentShieldIndex(5, 0,
                                                 &owner.container.items) == 1 &&
           items::LiveContainerParentShieldIndex(5, 1,
                                                 &owner.container.items) == 1;
  default:
    return false;
  }
}

void Missing(int id, LiveFixture &fixture) {
  if (id == 2934 || id == 2935 || id == 2938 || id == 2939) {
    expected = "missing inventory-storage owner";
    Menu_Framework_unusedInventoryStorage = nullptr;
    if (id == 2934)
      items::PreviewLiveTransferredMisc();
    else if (id == 2935)
      items::RefreshLiveMiscTransferCount();
    else if (id == 2938)
      items::SnapshotLiveGadgetBag();
    else
      items::CommitLiveGadgetBag();
  } else if (id == 2937) {
    expected = "missing container index owner";
    items::RebuildLiveContainerCategoryIndices(1, nullptr);
  } else {
    expected = "missing container record owner";
    if (id == 2936) {
      static_cast<void>(items::CopyLiveContainerItem(
          0x11, &fixture.storage->container.items, 0, nullptr));
    } else if (id == 2940) {
      static_cast<void>(items::LiveContainerParentWeaponIndex(1, 0, nullptr));
    } else {
      static_cast<void>(items::LiveContainerParentShieldIndex(5, 0, nullptr));
    }
  }
}

void Bounds(int id, LiveFixture &fixture) {
  auto &owner = *fixture.storage;
  expected = "record index outside container category storage";
  if (id == 2934 || id == 2935) {
    expected = "missing pending-transfer terminator";
    std::fill_n(owner.itemsToTransfer, 64, 0x101);
    owner.container.items.misc[255].id = 15;
    Menu_Container_transferredMiscCount = 91;
    diagnostic_state = [&] {
      return owner.transferPreview.misc[255].id == 15 &&
             Menu_Container_transferredMiscCount == 91;
    };
    if (id == 2934)
      items::PreviewLiveTransferredMisc();
    else
      items::RefreshLiveMiscTransferCount();
  } else if (id == 2936) {
    static_cast<void>(items::CopyLiveContainerItem(0x11, &owner.container.items,
                                                   64, &owner.stagedTransfer));
  } else if (id == 2937) {
    owner.container.indices.blades[0] = 1;
    owner.container.indices.blades[1] = 65;
    diagnostic_state = [&] { return owner.container.indices.blades[0] == 0; };
    items::RebuildLiveContainerCategoryIndices(1, &owner.container);
  } else if (id == 2938 || id == 2939) {
    expected = "missing or short inventory display order";
    Menu_Inventory_inventoryIndices[0] = nullptr;
    owner.gadgetBag.items.blades[63].id = 15;
    owner.gadgetBag.indices.blades[2] = 64;
    if (id == 2938) {
      diagnostic_state = [&] {
        return owner.gadgetBag.items.blades[63].id == 0 &&
               owner.gadgetBag.indices.blades[2] == 0;
      };
      items::SnapshotLiveGadgetBag();
    } else {
      diagnostic_state = [&] {
        return owner.gadgetBag.indices.blades[0] == 64 &&
               Audio_Seq_unusedInventory.blades[0].id == 0;
      };
      items::CommitLiveGadgetBag();
    }
  } else if (id == 2940) {
    static_cast<void>(
        items::LiveContainerParentWeaponIndex(1, 64, &owner.container.items));
  } else {
    static_cast<void>(
        items::LiveContainerParentShieldIndex(5, 192, &owner.container.items));
  }
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
  LiveFixture fixture;
  const int id = std::atoi(argv[1]);
  const std::string_view mode = argv[2];
  bool valid = false;
  if (mode == "current-owner" || mode == "positive") {
    valid = CurrentOwner(id, fixture);
  } else if (mode == "lazy") {
    valid = Lazy(id, fixture);
  } else if (mode == "alias") {
    valid = Alias(id, fixture);
  } else if (mode == "missing") {
    Missing(id, fixture);
  } else if (mode == "bounds") {
    Bounds(id, fixture);
  } else if (mode == "missing-other") {
    expected = "missing container record owner";
    Menu_Container_otherStorage = nullptr;
    items::PreviewLiveTransferredMisc();
  } else if (mode == "zero-blade" || mode == "zero-grip") {
    expected = "record index outside container category storage";
    auto &owner = *fixture.storage;
    owner.stagedTransfer.weapons[0].blade = mode == "zero-blade" ? 0 : 1;
    owner.stagedTransfer.weapons[0].grip = 0;
    owner.stagedTransfer.blades[0].id = 15;
    diagnostic_state = [&] {
      return owner.container.items.weapons[0].index == 1 &&
             owner.container.items.weapons[0].blade ==
                 (mode == "zero-blade" ? 0 : 1) &&
             (mode != "zero-grip" || owner.container.items.blades[0].id == 15);
    };
    static_cast<void>(items::CopyLiveContainerItem(0x10, &owner.container.items,
                                                   0, &owner.stagedTransfer));
  } else if (mode == "bad-gem") {
    expected = "record index outside container category storage";
    fixture.storage->stagedTransfer.shields[0].gems[0] = 193;
    diagnostic_state = [&] {
      return fixture.storage->container.items.shields[0].index == 1;
    };
    static_cast<void>(
        items::CopyLiveContainerItem(0x13, &fixture.storage->container.items, 0,
                                     &fixture.storage->stagedTransfer));
  } else if (mode == "bad-capacity") {
    expected = "capacity exceeds container category storage";
    Menu_Container_containerItemCapacities[1] = 65;
    if (id == 2936) {
      static_cast<void>(
          items::CopyLiveContainerItem(0x11, &fixture.storage->container.items,
                                       0, &fixture.storage->stagedTransfer));
    } else {
      items::RebuildLiveContainerCategoryIndices(1,
                                                 &fixture.storage->container);
    }
  } else if (mode == "bad-category") {
    expected = "category outside container categories";
    if (id == 2936) {
      static_cast<void>(
          items::CopyLiveContainerItem(0x18, nullptr, 0, nullptr));
    } else {
      items::RebuildLiveContainerCategoryIndices(-1, nullptr);
    }
  } else if (mode == "low-index") {
    expected = "record index outside container category storage";
    if (id == 2940) {
      static_cast<void>(items::LiveContainerParentWeaponIndex(
          2, -1, &fixture.storage->container.items));
    } else {
      static_cast<void>(items::LiveContainerParentShieldIndex(
          5, -1, &fixture.storage->container.items));
    }
  }
  std::printf("boundaries=%s\n", valid && expected.empty() ? "PASS" : "FAIL");
  return valid && expected.empty() ? 0 : 86;
}
