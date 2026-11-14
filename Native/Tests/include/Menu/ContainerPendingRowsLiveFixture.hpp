#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include "Core/Memory/HeapLive.hpp"
#include "Input/ControllerLive.hpp"
#include "Input/LegacyPad.hpp"
#include "Items/ContainerTransfersLiveFixture.hpp"
#include "Menu/Container/ContainerPendingRowsLive.hpp"
#include "Menu/ContainerPendingRowsWorld.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Boot/OverlayReset.h"
#include "Legacy/Input/LibPad.h"
#include "State/Timers.h"
}

namespace container_pending_rows_live_tests {
namespace rows = VS::Menu::container;
namespace framework = VS::Menu::Framework;
namespace equipment = equipment_detail_tests;
namespace data = container_pending_rows_tests;
inline constexpr std::size_t kTextsPerRow = 2;
inline constexpr std::size_t kHeapBytes = 0x40000;
inline constexpr int kListMenu = 81;
inline constexpr int kListInfo = 0x19200;
inline constexpr int kSettleFrames = 48;
inline constexpr int kFirstListRow = 20;
inline constexpr int kTextOrderingEntries = 34;
inline constexpr std::size_t kPacketLimit = 4096;
inline constexpr std::size_t kHelpPayloadBytes = 27580;
inline constexpr std::size_t kOldAllocationBytes = 0x6C00;
inline constexpr std::size_t kOldTextOffset = 0x6400;
inline constexpr std::size_t kOldTypeOffset = 0x6000;

using RowWorkspace = rows::ItemRowsWorkspace;
static_assert(offsetof(RowWorkspace, types) == kOldTypeOffset);
static_assert(offsetof(RowWorkspace, texts) == kOldTextOffset);

struct PendingRowsLiveFixture : equipment::LiveFixture {
  container_transfers_tests::LiveFixture containers;
  std::vector<std::byte> heap_bytes = std::vector<std::byte>(kHeapBytes);
  VS::Core::Memory::ScopedLiveGameHeap heap{heap_bytes};
  equipment::ValueRestore<int*> saved_types{Menu_Framework_rowTypeBuf};
  equipment::ValueRestore<int> saved_previous{Input_previousButtons};
  equipment::ValueRestore<decltype(State_tickSpeed)> saved_tick{
      State_tickSpeed};
  equipment::ArrayRestore<u_int, std::size(Input_buttonHeldFrames)> saved_held{
      Input_buttonHeldFrames};
  equipment::ValueRestore<u_long*> saved_text_ordering{
      Menu_activeTextBoxOrderingTable};
  std::array<int, framework::kRowTypeCount> row_types{};
  RowWorkspace workspace;

  PendingRowsLiveFixture() {
    framework::ResetLiveMainMenuStatics();
    CopyText(R"(Native\data\disc\MENU\ITEMHELP.BIN)",
             std::as_writable_bytes(std::span(help)), kHelpPayloadBytes);
    CopyText(R"(Native\data\disc\MENU\ITEMNAME.BIN)",
             std::as_writable_bytes(std::span(names)),
             framework::kItemNamesBytes);
    Menu_Framework_rowTypeBuf = row_types.data();
    Menu_Container_containerData = &containers.storage->container;
    Menu_Container_otherStorage = &containers.storage->container;
    Input_previousButtons = 0;
    std::ranges::fill(Input_buttonHeldFrames, 0U);
    State_tickSpeed = 2;
    Menu_activeTextBoxOrderingTable = Menu_textBoxOrderingTables;
    ClearOTag(Menu_activeTextBoxOrderingTable, kTextOrderingEntries);
    PadInitDirect(
        reinterpret_cast<unsigned char*>(Input_legacyPadResponseBytes[0]),
        reinterpret_cast<unsigned char*>(Input_legacyPadResponseBytes[1]));
    Buttons(0);
  }
  ~PendingRowsLiveFixture() {
    framework::ResetLiveMainMenuStatics();
    PadInitDirect(nullptr, nullptr);
  }
  static void CopyText(const char* path, std::span<std::byte> target,
                       std::size_t expected_bytes) {
    const auto source = data::ReadText(path);
    if (source.size() != expected_bytes || source.size() > target.size()) {
      throw std::runtime_error("original item text has an unexpected extent");
    }
    std::ranges::fill(target, std::byte{0});
    std::ranges::transform(source, target.begin(),
                           [](std::uint8_t value) { return std::byte{value}; });
  }
  static void Buttons(std::uint32_t buttons) {
    VS::Legacy::FeedPad(0, VS::Input::PadState{.held = buttons, .sticks = {}},
                        true);
    VS::Legacy::FeedPad(1, {}, false);
    VS::Input::UpdateLiveControllers();
  }
  int Build(int category) {
    return rows::BuildLiveItemRows(category, workspace.texts.data(),
                                   workspace.types.data(),
                                   workspace.descriptions.data());
  }
  void Show(int category, int count) {
    framework::CreateLiveMenuRows(count, (category + kListMenu) | kListInfo,
                                  workspace.texts.data(),
                                  workspace.types.data());
  }
  int Frame(std::uint32_t buttons = 0) {
    ResetFrame();
    Buttons(buttons);
    framework::UpdateLiveScreen();
    Menu_UpdateTextBoxes();
    return framework::LiveSelectedRow();
  }
  void Settle() {
    for (int tick = 0; tick < kSettleFrames; ++tick) Frame();
  }
  void AddMisc() {
    auto& owner = containers.storage->container;
    owner.items.misc[0] = {.id = data::kMiscId, .count = 1, .index = 1};
    std::span(owner.items.misc).back() = {
        .id = data::kMiscId + 1, .count = 2, .index = 0};
    owner.indices.misc[0] = 1;
    owner.indices.misc[1] = data::kRowCount;
  }
  bool OwnsWords(const u_long* pointer, std::size_t count) const {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const auto bytes = count * sizeof(*pointer);
    const auto within = [address, bytes](const void* first, const void* last) {
      const auto begin = reinterpret_cast<std::uintptr_t>(first);
      const auto end = reinterpret_cast<std::uintptr_t>(last);
      return address >= begin && address <= end && bytes <= end - address;
    };
    return within(primitives.data() + 1, Render_frameScratch.primitiveCursor) ||
           within(ordering.data(), ordering.data() + ordering.size()) ||
           within(Menu_textBoxOrderingTables,
                  Menu_textBoxOrderingTables +
                      std::size(Menu_textBoxOrderingTables));
  }
  std::vector<std::vector<std::uint32_t>> Packets() const {
    const auto cursor =
        reinterpret_cast<std::uintptr_t>(Render_frameScratch.primitiveCursor);
    const auto first = reinterpret_cast<std::uintptr_t>(primitives.data() + 1);
    const auto last = reinterpret_cast<std::uintptr_t>(primitives.data() +
                                                       primitives.size() - 1);
    if (cursor < first || cursor > last ||
        (cursor - first) % sizeof(u_long) != 0) {
      throw std::runtime_error("physical packet cursor escaped its allocation");
    }
    std::vector<std::vector<std::uint32_t>> packets;
    const void* packet = &ordering.back();
    while (packet != nullptr && packets.size() < kPacketLimit) {
      const auto* words = static_cast<const u_long*>(packet);
      if (!OwnsWords(words, 1)) {
        throw std::runtime_error(
            "physical ordering tag escaped its actual owner");
      }
      const auto length = static_cast<const P_TAG*>(packet)->len;
      if (!OwnsWords(words, static_cast<std::size_t>(length) + 1U)) {
        throw std::runtime_error(
            "physical packet payload escaped its actual owner");
      }
      packets.emplace_back(words, words + length + 1U);
      packet = isendprim(packet) ? nullptr : nextPrim(packet);
    }
    if (packet != nullptr) {
      throw std::runtime_error("cyclic physical ordering chain");
    }
    return packets;
  }
};
}  // namespace container_pending_rows_live_tests
