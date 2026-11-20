#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "Battle/Actor.hpp"
#include "Boot/OverlayReset.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Input/ControllerLive.hpp"
#include "Input/LegacyPad.hpp"
#include "Items/ContainerTransfersLive.hpp"
#include "Items/ContainerTransfersLiveFixture.hpp"
#include "Items/InventoryLive.hpp"
#include "Legacy/Input/LibPad.hpp"
#include "Menu/Container/ContainerListTransferLive.hpp"
#include "Menu/Container/ContainerMenuDialogsLive.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Inventory/EquipmentMenusLive.hpp"
#include "Menu/Inventory/Inventory.hpp"
#include "Menu/TextBoxUpdateLive.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/Timers.hpp"

namespace container_dialog_live_tests {
namespace dialogs = VS::Menu::container;
namespace framework = VS::Menu::Framework;
namespace equipment = equipment_detail_tests;
inline constexpr std::size_t kDialogHeapBytes = 0x40000;
inline constexpr int kDialogFrames = 48;
inline constexpr int kQuantityRow = 34;
inline constexpr int kQuantityY = 130;
inline constexpr std::size_t kMenuTextOffset = 0x6D2C;
inline constexpr std::size_t kVisitLimit = 1024;
inline constexpr int kTextOrderingTags = 34;
inline constexpr int kMiscItemId = 321;
inline constexpr int kOtherMiscItemId = 322;
inline constexpr int kMiscSortMenu = 7;
inline constexpr int kLargeMiscCount = 99;
inline constexpr int kLastMiscToken = 256;
inline constexpr int kMultipleDiscardCommand = 0x10000;
inline constexpr int kMiscDiscardCategory = 0x6000;
inline constexpr int kInformationWaitMultiplier = 4;
inline constexpr int kInformationBox = 7;
using TickFunction = int (*)(int);

struct DialogLiveFixture : equipment::LiveFixture {
  container_transfers_tests::LiveFixture containers;
  std::vector<std::byte> heap_bytes = std::vector<std::byte>(kDialogHeapBytes);
  VS::Core::Memory::ScopedLiveGameHeap heap{heap_bytes};
  equipment::ValueRestore<int> saved_previous{VS::Input::previousButtons};
  equipment::ValueRestore<decltype(VS::State::tickSpeed)> saved_tick{
      VS::State::tickSpeed};
  equipment::ArrayRestore<u_int, std::size(VS::Input::buttonHeldFrames)>
      saved_held{VS::Input::buttonHeldFrames};
  equipment::ArrayRestore<std::uint16_t,
                          std::size(VS::Menu::container::menuText)>
      saved_container_text{VS::Menu::container::menuText};
  equipment::ValueRestore<std::uint8_t> saved_direction{
      VS::Menu::container::browsingGadgetBag};
  equipment::ValueRestore<std::uint8_t> saved_controls{
      VS::Menu::container::controlsPanelMode};
  equipment::ValueRestore<std::uint8_t> saved_freeze{
      VS::Menu::Inventory::freezeTabArrows};
  equipment::ValueRestore<char> saved_animation{
      VS::Menu::Framework::itemsListRow};
  equipment::ValueRestore<u_long*> saved_text_ordering{
      VS::Menu::activeTextBoxOrderingTable};
  equipment::ValueRestore<char> saved_text_index{VS::Menu::textBoxBufferIndex};
  equipment::ArrayRestore<u_long, std::size(VS::Menu::textBoxOrderingTables)>
      saved_text_tables{VS::Menu::textBoxOrderingTables};

  DialogLiveFixture() {
    framework::ResetLiveMainMenuStatics();
    dialogs::ResetLiveContainerDialogSession();
    LoadDialogTexts();
    VS::Menu::container::containerData = &containers.storage->container;
    VS::Menu::container::otherStorage = &containers.storage->gadget_bag;
    VS::Menu::container::browsingGadgetBag = 0;
    VS::Input::previousButtons = 0;
    std::ranges::fill(VS::Input::buttonHeldFrames, 0U);
    VS::State::tickSpeed = 2;
    VS::Menu::textBoxBufferIndex = 0;
    VS::Menu::activeTextBoxOrderingTable = VS::Menu::textBoxOrderingTables;
    ClearOTag(VS::Menu::activeTextBoxOrderingTable, kTextOrderingTags);
    VS::Legacy::PadInitDirect(VS::Input::legacyPadResponseBytes[0].data(),
                              VS::Input::legacyPadResponseBytes[1].data());
    Buttons(0);
  }
  ~DialogLiveFixture() {
    framework::ResetLiveMainMenuStatics();
    dialogs::ResetLiveContainerDialogSession();
    VS::Legacy::PadInitDirect(nullptr, nullptr);
  }
  static std::vector<unsigned char> ReadData(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("missing real dialog text input");
    return {std::istreambuf_iterator<char>(stream), {}};
  }
  void LoadDialogTexts() {
    const auto text = ReadData(R"(Native\data\disc\MENU\ITEMHELP.BIN)");
    auto output = std::as_writable_bytes(std::span(help));
    if (text.size() > output.size()) {
      throw std::runtime_error(
          "original help exceeds the production allocation");
    }
    std::ranges::fill(output, std::byte{0});
    std::ranges::transform(text, output.begin(),
                           [](unsigned char byte) { return std::byte{byte}; });
    const auto module = ReadData(R"(Native\data\disc\MENU\MENUD.PRG)");
    auto menu =
        std::as_writable_bytes(std::span(VS::Menu::container::menuText));
    if (kMenuTextOffset + menu.size() > module.size()) {
      throw std::runtime_error("short original container dialog text");
    }
    std::ranges::transform(
        std::span(module).subspan(kMenuTextOffset, menu.size()), menu.begin(),
        [](unsigned char byte) { return std::byte{byte}; });
  }
  static void Buttons(std::uint32_t buttons) {
    VS::Legacy::FeedPad(0, VS::Input::PadState{.held = buttons, .sticks = {}},
                        true);
    VS::Legacy::FeedPad(1, {}, false);
    VS::Input::UpdateLiveControllers();
  }
  int Frame(TickFunction update, std::uint32_t buttons = 0) {
    ResetFrame();
    Buttons(buttons);
    const int result = update(0);
    framework::UpdateLiveScreen();
    VS::Menu::UpdateLiveTextBoxes();
    return result;
  }
  int RunFrames(TickFunction update, int frames = kDialogFrames) {
    int result = 0;
    for (int tick = 0; tick < frames; ++tick) {
      const auto current = Frame(update);
      if (current != 0) result = current;
    }
    return result;
  }
  void OpenMiscSort() {
    auto& owner = containers.storage->container;
    owner.items.misc[0] = {.id = kMiscItemId, .count = 1, .index = 1};
    std::span(owner.items.misc).back() = {
        .id = kOtherMiscItemId, .count = kLargeMiscCount, .index = 0};
    owner.indices.misc[0] = 1;
    owner.indices.misc[1] = kLastMiscToken;
    if (dialogs::UpdateLiveSortMenu(kMiscSortMenu) != 0) {
      throw std::runtime_error("sort initializer unexpectedly completed");
    }
    RunFrames(dialogs::UpdateLiveSortMenu);
  }
  void OpenQuantity(std::uint8_t count, int one_based = kLastMiscToken) {
    auto& stack = VS::Menu::container::containerData->items.misc[one_based - 1];
    stack.id = kMiscItemId;
    stack.count = static_cast<char>(count);
    const int command =
        kMultipleDiscardCommand | kMiscDiscardCategory | one_based;
    if (dialogs::UpdateLiveDiscardMenu(command) != 0) {
      throw std::runtime_error(
          "discard initializer returned a completed result");
    }
    for (int tick = 0; tick < kDialogFrames * kInformationWaitMultiplier;
         ++tick) {
      Frame(dialogs::UpdateLiveDiscardMenu);
      if (static_cast<std::uint8_t>(rows[kQuantityRow].text[2]) ==
          VS::Render::kTextSpaceGlyph) {
        return;
      }
    }
    throw std::runtime_error(
        "actual information/row progression did not open quantity input");
  }
  std::vector<std::vector<std::uint32_t>> Packets() const {
    std::vector<std::vector<std::uint32_t>> packets;
    const void* packet = &ordering.back();
    for (std::size_t count = 0; packet != nullptr && count < kVisitLimit;
         ++count) {
      const auto* words = static_cast<const u_long*>(packet);
      if (!OwnsWords(words, 1)) {
        throw std::runtime_error(
            "ordering tag is outside its real frame owner");
      }
      const auto length = static_cast<const P_TAG*>(packet)->len;
      if (!OwnsWords(words, static_cast<std::size_t>(length) + 1U)) {
        throw std::runtime_error(
            "declared packet payload exceeds its real owner");
      }
      packets.emplace_back(words, words + length + 1U);
      packet = isendprim(packet) ? nullptr : nextPrim(packet);
    }
    if (packet != nullptr) throw std::runtime_error("cyclic ordering chain");
    return packets;
  }
  bool OwnsWords(const u_long* pointer, std::size_t words) const {
    const auto first = reinterpret_cast<std::uintptr_t>(pointer);
    const auto primitive =
        reinterpret_cast<std::uintptr_t>(primitives.data() + 1);
    const auto cursor = reinterpret_cast<std::uintptr_t>(
        VS::Render::FrameScratchpad().primitive_cursor);
    const auto table = reinterpret_cast<std::uintptr_t>(ordering.data());
    const auto text_table =
        reinterpret_cast<std::uintptr_t>(VS::Menu::textBoxOrderingTables);
    const auto bytes = words * sizeof(u_long);
    if (first >= primitive && first <= cursor && bytes <= cursor - first) {
      return true;
    }
    if (first >= table && first <= table + sizeof ordering &&
        bytes <= table + sizeof ordering - first) {
      return true;
    }
    return first >= text_table &&
           first <= text_table + sizeof VS::Menu::textBoxOrderingTables &&
           bytes <= text_table + sizeof VS::Menu::textBoxOrderingTables - first;
  }
};

struct EquippedFixture : DialogLiveFixture {
  equipment::ValueRestore<VS::Battle::Actor*> saved_actor{
      VS::Battle::characterState};
  equipment::ValueRestore<int*> saved_types{VS::Menu::Framework::rowTypeBuf};
  std::array<int, framework::kRowTypeCount> row_types{};
  VS::Battle::Actor actor{};
  VS::Battle::ActorStats stats{};

  EquippedFixture() {
    actor.stats = &stats;
    VS::Battle::characterState = &actor;
    VS::Menu::Framework::rowTypeBuf = row_types.data();
    AddWeapon();
    const auto module = ReadData(R"(Native\data\disc\MENU\MENU3.PRG)");
    const auto entries = VS::Content::ModuleDataEntries();
    const auto text = std::ranges::find_if(entries, [](const auto& entry) {
      return entry.target ==
                 static_cast<void*>(VS::Menu::Inventory::menuText) &&
             std::string_view(entry.module) == "MENU/MENU3.PRG";
    });
    if (text == entries.end() ||
        !VS::Content::CopyModuleDataEntry(*text, module)) {
      throw std::runtime_error(
          "missing original inventory equipment-menu text");
    }
  }
  void EquipThroughProductionMenu() {
    if (VS::Menu::Inventory::UpdateLiveWeaponMenu(1) != 0) {
      throw std::runtime_error(
          "weapon menu initializer unexpectedly completed");
    }
    RunFrames(VS::Menu::Inventory::UpdateLiveWeaponMenu);
    Frame(VS::Menu::Inventory::UpdateLiveWeaponMenu, VS::Input::Down);
    Frame(VS::Menu::Inventory::UpdateLiveWeaponMenu);
    Frame(VS::Menu::Inventory::UpdateLiveWeaponMenu, VS::Input::Circle);
    if (VS::Items::inventory.weapons[0].is_equipped == 0) {
      throw std::runtime_error(
          "real equipment-menu input did not equip the weapon");
    }
    VS::Items::SnapshotLiveGadgetBag();
  }
  void OpenEquippedContainerActions() {
    framework::ClearLiveMenuExcept(framework::kNoRow);
    if (VS::Menu::container::LiveUpdateItemList(2) != 0) {
      throw std::runtime_error(
          "container list initializer unexpectedly completed");
    }
    RunFrames(VS::Menu::container::LiveUpdateItemList);
    Frame(VS::Menu::container::LiveUpdateItemList, VS::Input::Right);
    RunFrames(VS::Menu::container::LiveUpdateItemList);
    Frame(VS::Menu::container::LiveUpdateItemList, VS::Input::Circle);
    RunFrames(VS::Menu::container::LiveUpdateItemList);
    Frame(VS::Menu::container::LiveUpdateItemList, VS::Input::Circle);
    RunFrames(VS::Menu::container::LiveUpdateItemList);
  }
};
}  // namespace container_dialog_live_tests
