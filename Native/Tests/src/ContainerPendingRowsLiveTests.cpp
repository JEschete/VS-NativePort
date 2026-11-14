#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Menu/ContainerPendingRowsLiveFixture.hpp"

extern "C" {
#include "Menu/Container/ItemView.h"
}

namespace {
namespace tests = container_pending_rows_live_tests;
namespace data = container_pending_rows_tests;
namespace rows = VS::Menu::container;
namespace framework = VS::Menu::Framework;
using LiveFixture = tests::PendingRowsLiveFixture;
constexpr int kMiscCategory = 6;
constexpr int kPendingCategory = 7;
constexpr int kLastPendingPosition = 64;
constexpr std::uint16_t kEmptyNeighbor = 0x102;
constexpr std::uint16_t kFirstListRowY = 50;
constexpr std::uint32_t kNameGlyphCommand = 0x66;
constexpr std::uint32_t kMenuGlyphSize = 0x000C000C;
constexpr std::uint32_t kMenuGlyphDrawMode = 0xE100002D;

std::vector<std::vector<std::uint32_t>> VariableGlyphs(
    const LiveFixture& fixture) {
  std::vector<std::vector<std::uint32_t>> result;
  for (const auto& packet : fixture.Packets()) {
    if (packet.size() == VS::Render::kSpritePacketWords &&
        packet[VS::Render::kSpriteColorWord] >>
                VS::Render::kPacketLengthShift ==
            kNameGlyphCommand &&
        packet[VS::Render::kSpriteDrawModeWord] == kMenuGlyphDrawMode) {
      result.push_back(packet);
    }
  }
  return result;
}
}  // namespace

TEST_CASE("F02979 live searches the current workspace and all 64 entries",
          "[container-pending-rows-live][F02979]") {
  LiveFixture fixture;
  std::span(fixture.containers.storage->itemsToTransfer).back() =
      data::kLastGemPendingToken;
  REQUIRE(rows::FindLivePendingTransfer(5, data::kLastGemSlot) ==
          kLastPendingPosition);
  Menu_Framework_unusedInventoryStorage = fixture.containers.replacement.get();
  fixture.containers.replacement->itemsToTransfer[1] =
      data::kLastGemPendingToken;
  REQUIRE(rows::FindLivePendingTransfer(5, data::kLastGemSlot) == 2);
  REQUIRE(rows::FindLivePendingTransfer(0, -1) == 1);
}

TEST_CASE(
    "F02980 live appends to the current owner without clearing its source",
    "[container-pending-rows-live][F02980]") {
  LiveFixture fixture;
  rows::AppendLivePendingTransfer(kMiscCategory, data::kLastMiscSlot);
  REQUIRE(fixture.containers.storage->itemsToTransfer[0] ==
          data::kLastMiscPendingToken);
  Menu_Framework_unusedInventoryStorage = fixture.containers.replacement.get();
  rows::AppendLivePendingTransfer(1, 0);
  REQUIRE(fixture.containers.replacement->itemsToTransfer[0] ==
          data::kBladePendingToken);
  REQUIRE(fixture.containers.storage->itemsToTransfer[0] ==
          data::kLastMiscPendingToken);
}

TEST_CASE("F02981 live prunes stale entries and appends actual staged records",
          "[container-pending-rows-live][F02981]") {
  LiveFixture fixture;
  auto& storage = *fixture.containers.storage;
  storage.itemsToTransfer[0] = kEmptyNeighbor;
  storage.itemsToTransfer[2] = data::kMiscPendingToken;
  storage.stagedTransfer.misc[0].id = data::kMiscId;
  storage.stagedTransfer.blades[0].id = 1;
  rows::RebuildLivePendingTransfers();
  REQUIRE(storage.itemsToTransfer[0] == data::kMiscPendingToken);
  REQUIRE(storage.itemsToTransfer[1] == data::kBladePendingToken);
  REQUIRE(storage.itemsToTransfer[2] == 0);
  REQUIRE(storage.stagedTransfer.misc[0].id == data::kMiscId);
}

TEST_CASE(
    "F02981 live rebuilding and row formatting use replacement workspaces on "
    "reentry",
    "[container-pending-rows-live][F02981][F02982]") {
  LiveFixture fixture;
  auto& replacement = *fixture.containers.replacement;
  replacement.stagedTransfer.misc[0] = {
      .id = data::kMiscId + 1, .count = 2, .index = 1};
  Menu_Framework_unusedInventoryStorage = &replacement;
  rows::RebuildLivePendingTransfers();
  REQUIRE(fixture.Build(kPendingCategory) == 1);
  REQUIRE(replacement.itemsToTransfer[0] == data::kMiscPendingToken);
  REQUIRE(fixture.workspace.texts[0] ==
          Menu_Framework_itemNames[data::kMiscId + 1]);
  REQUIRE(fixture.containers.storage->itemsToTransfer[0] == 0);
}

TEST_CASE(
    "F02982 live retains actual borrowed weapon names and equipped "
    "restrictions",
    "[container-pending-rows-live][F02982]") {
  LiveFixture fixture;
  data::Records source;
  data::Populate(source);
  auto& owner = fixture.containers.storage->container;
  VS::Items::Live::Commit(owner.items.weapons[0],
                          source.storage.items.weapons[0]);
  VS::Items::Live::Commit(owner.items.blades[0],
                          source.storage.items.blades[0]);
  VS::Items::Live::Commit(owner.items.grips[0], source.storage.items.grips[0]);
  owner.items.weapons[0].isEquipped = 1;
  owner.indices.weapons[0] = 1;
  REQUIRE(fixture.Build(0) == 1);
  REQUIRE(fixture.workspace.texts[0] == owner.items.weapons[0].name);
  REQUIRE((fixture.workspace.types[0] & data::kRestrictionMask) ==
          data::kEquipped);
  REQUIRE(fixture.workspace.texts[1] == fixture.workspace.descriptions.data());
}

TEST_CASE(
    "F02982 live pending attachments use staged rather than selected records",
    "[container-pending-rows-live][F02982]") {
  LiveFixture fixture;
  data::Records source;
  data::Populate(source);
  auto blade = source.storage.items.blades[0];
  blade.assembled_weapon_index = 1;
  VS::Items::Live::Commit(fixture.containers.storage->stagedTransfer.blades[0],
                          blade);
  fixture.containers.storage->itemsToTransfer[0] = data::kBladePendingToken;
  REQUIRE(fixture.Build(kPendingCategory) == 1);
  REQUIRE((fixture.workspace.types[0] & data::kRestrictionMask) ==
          data::kPendingAttached);
  REQUIRE(fixture.containers.storage->container.items.blades[0].id == 0);
}

TEST_CASE(
    "F02982 live formats the full 256-row caller workspace with real item "
    "texts",
    "[container-pending-rows-live][F02982]") {
  LiveFixture fixture;
  auto& owner = fixture.containers.storage->container;
  for (std::size_t slot = 0; slot < data::kRowCount; ++slot) {
    owner.items.misc[slot].id = data::kMiscId;
    owner.indices.misc[slot] = static_cast<std::uint16_t>(slot + 1);
  }
  REQUIRE(fixture.Build(kMiscCategory) == data::kRowCount);
  REQUIRE(fixture.workspace.texts[fixture.workspace.texts.size() - 2] ==
          Menu_Framework_itemNames[data::kMiscId]);
  REQUIRE(fixture.workspace.texts.back() ==
          fixture.workspace.descriptions.data() +
              (data::kRowCount - 1) * framework::kRowDescriptionBufferBytes);
}

TEST_CASE("F02985 controller allocates the full native row workspace",
          "[container-pending-rows-live][F02985]") {
  static_assert(sizeof(tests::RowWorkspace) > tests::kOldAllocationBytes);
  LiveFixture fixture;
  constexpr std::size_t kOutsideBytes = 4096;
  constexpr std::byte kCanary{0xA5};
  std::vector<std::byte> backing(tests::kHeapBytes + kOutsideBytes, kCanary);
  VS::Core::Memory::ScopedLiveGameHeap bounded{
      std::span(backing).first(tests::kHeapBytes)};
  Boot_ResetMenudStatics();
  for (auto& item : fixture.containers.storage->container.items.misc) {
    item.id = data::kMiscId;
    item.count = 1;
  }
  REQUIRE(Menu_Container_UpdateItemList(1) == 0);
  Menu_Framework_menuMemory.opaqueA0[0] = 7;
  const auto guard_before = VS::Core::Memory::DamagedLiveHeapGuardCount();
  REQUIRE(Menu_Container_UpdateItemList(0) == 0);
  CHECK(VS::Core::Memory::DamagedLiveHeapGuardCount() == guard_before);
  CHECK(std::ranges::all_of(std::span(backing).subspan(tests::kHeapBytes),
                            [](std::byte value) { return value == kCanary; }));
}

TEST_CASE(
    "F02982 actual menu rows consume formatted data and real controller "
    "selection",
    "[container-pending-rows-live][F02982]") {
  LiveFixture fixture;
  fixture.AddMisc();
  const auto count = fixture.Build(kMiscCategory);
  REQUIRE(count == 2);
  fixture.Show(kMiscCategory, count);
  fixture.Settle();
  REQUIRE(fixture.rows[tests::kFirstListRow].state ==
          Menu_Framework_MenuItemState_Idle);
  fixture.Frame(VS::Input::Down);
  fixture.Frame();
  REQUIRE(fixture.Frame(VS::Input::Circle) == 1);
  REQUIRE((Input_buttonsPressed.allButtons & VS::Input::Circle) != 0);
}

TEST_CASE(
    "F02982 actual font output has bounded physical packet payloads and "
    "ordering links",
    "[container-pending-rows-live][F02982]") {
  LiveFixture fixture;
  fixture.AddMisc();
  fixture.Show(kMiscCategory, fixture.Build(kMiscCategory));
  fixture.Settle();
  const auto glyphs = VariableGlyphs(fixture);
  REQUIRE_FALSE(glyphs.empty());
  REQUIRE(std::ranges::all_of(glyphs, [](const auto& packet) {
    return packet[0] >> VS::Render::kPacketLengthShift ==
               VS::Render::kSpritePacketWords - 1 &&
           packet[VS::Render::kSpriteTextureWord] != 0 &&
           packet[VS::Render::kSpriteSizeWord] == kMenuGlyphSize &&
           packet[VS::Render::kSpriteDrawModeWord] == kMenuGlyphDrawMode;
  }));
  REQUIRE(std::ranges::any_of(glyphs, [](const auto& packet) {
    return static_cast<std::uint16_t>(packet[VS::Render::kSpritePositionWord] >>
                                      framework::kHalfwordShift) ==
           kFirstListRowY;
  }));
}

TEST_CASE(
    "Ordinary MENUD reload retains pending records and current native owners",
    "[container-pending-rows-live][F02979][F02980][F02981][F02982]") {
  LiveFixture fixture;
  fixture.AddMisc();
  auto& storage = *fixture.containers.storage;
  storage.stagedTransfer.misc[0].id = data::kMiscId;
  storage.itemsToTransfer[0] = data::kMiscPendingToken;
  const auto* const owner = Menu_Container_containerData;
  Boot_ResetMenudStatics();
  REQUIRE(Menu_Framework_unusedInventoryStorage == &storage);
  REQUIRE(Menu_Container_containerData == owner);
  REQUIRE(rows::FindLivePendingTransfer(kMiscCategory, 0) == 1);
  rows::AppendLivePendingTransfer(kMiscCategory, data::kLastMiscSlot);
  rows::RebuildLivePendingTransfers();
  REQUIRE(fixture.Build(kPendingCategory) == 1);
  REQUIRE(storage.itemsToTransfer[0] == data::kMiscPendingToken);
  REQUIRE(storage.itemsToTransfer[1] == 0);
}
