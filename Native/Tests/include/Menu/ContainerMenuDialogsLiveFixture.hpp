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

#include "Core/Memory/HeapLive.hpp"
#include "Input/ControllerLive.hpp"
#include "Input/LegacyPad.hpp"
#include "Items/ContainerTransfersLive.hpp"
#include "Items/ContainerTransfersLiveFixture.hpp"
#include "Menu/Container/ContainerMenuDialogsLive.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Battle/Actor.h"
#include "Boot/OverlayReset.h"
#include "Legacy/Input/LibPad.h"
#include "State/Timers.h"
}

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
  equipment::ValueRestore<int> saved_previous{Input_previousButtons};
  equipment::ValueRestore<decltype(State_tickSpeed)> saved_tick{
      State_tickSpeed};
  equipment::ArrayRestore<u_int, std::size(Input_buttonHeldFrames)> saved_held{
      Input_buttonHeldFrames};
  equipment::ArrayRestore<u_short, std::size(Menu_Container_menuText)>
      saved_container_text{Menu_Container_menuText};
  equipment::ValueRestore<u_char> saved_direction{
      Menu_Container_browsingGadgetBag};
  equipment::ValueRestore<char> saved_controls{
      Menu_Container_controlsPanelMode};
  equipment::ValueRestore<char> saved_freeze{Menu_Inventory_freezeTabArrows};
  equipment::ValueRestore<char> saved_animation{Menu_Framework_itemsListRow};
  equipment::ValueRestore<u_long*> saved_text_ordering{
      Menu_activeTextBoxOrderingTable};

  DialogLiveFixture() {
    framework::ResetLiveMainMenuStatics();
    dialogs::ResetLiveContainerDialogSession();
    LoadDialogTexts();
    Menu_Container_containerData = &containers.storage->container;
    Menu_Container_otherStorage = &containers.storage->gadgetBag;
    Menu_Container_browsingGadgetBag = 0;
    Input_previousButtons = 0;
    std::ranges::fill(Input_buttonHeldFrames, 0U);
    State_tickSpeed = 2;
    Menu_activeTextBoxOrderingTable = Menu_textBoxOrderingTables;
    ClearOTag(Menu_activeTextBoxOrderingTable, kTextOrderingTags);
    PadInitDirect(
        reinterpret_cast<unsigned char*>(Input_legacyPadResponseBytes[0]),
        reinterpret_cast<unsigned char*>(Input_legacyPadResponseBytes[1]));
    Buttons(0);
  }
  ~DialogLiveFixture() {
    framework::ResetLiveMainMenuStatics();
    dialogs::ResetLiveContainerDialogSession();
    PadInitDirect(nullptr, nullptr);
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
    auto menu = std::as_writable_bytes(std::span(Menu_Container_menuText));
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
    Menu_UpdateTextBoxes();
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
    auto& stack = Menu_Container_containerData->items.misc[one_based - 1];
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
    const auto cursor =
        reinterpret_cast<std::uintptr_t>(Render_frameScratch.primitiveCursor);
    const auto table = reinterpret_cast<std::uintptr_t>(ordering.data());
    const auto text_table =
        reinterpret_cast<std::uintptr_t>(Menu_textBoxOrderingTables);
    const auto bytes = words * sizeof(u_long);
    if (first >= primitive && first <= cursor && bytes <= cursor - first) {
      return true;
    }
    if (first >= table && first <= table + sizeof ordering &&
        bytes <= table + sizeof ordering - first) {
      return true;
    }
    return first >= text_table &&
           first <= text_table + sizeof Menu_textBoxOrderingTables &&
           bytes <= text_table + sizeof Menu_textBoxOrderingTables - first;
  }
};

struct EquippedFixture : DialogLiveFixture {
  equipment::ValueRestore<Battle_Actor*> saved_actor{Battle_characterState};
  equipment::ValueRestore<int*> saved_types{Menu_Framework_rowTypeBuf};
  std::array<int, framework::kRowTypeCount> row_types{};
  Battle_Actor actor{};
  Battle_ActorStats stats{};

  EquippedFixture() {
    actor.stats = &stats;
    Battle_characterState = &actor;
    Menu_Framework_rowTypeBuf = row_types.data();
    AddWeapon();
    const auto module = ReadData(R"(Native\data\disc\MENU\MENU3.PRG)");
    const auto entries = std::span(Content_moduleData, Content_moduleDataCount);
    const auto text = std::ranges::find_if(entries, [](const auto& entry) {
      return entry.target == static_cast<void*>(Menu_Inventory_menuText) &&
             std::string_view(entry.module) == "MENU/MENU3.PRG";
    });
    if (text == entries.end() ||
        !VS::Content::CopyModuleDataEntry(*text, module)) {
      throw std::runtime_error(
          "missing original inventory equipment-menu text");
    }
  }
  void EquipThroughProductionMenu() {
    if (Menu_Inventory_UpdateWeaponMenu(1) != 0) {
      throw std::runtime_error(
          "weapon menu initializer unexpectedly completed");
    }
    RunFrames(Menu_Inventory_UpdateWeaponMenu);
    Frame(Menu_Inventory_UpdateWeaponMenu, VS::Input::Down);
    Frame(Menu_Inventory_UpdateWeaponMenu);
    Frame(Menu_Inventory_UpdateWeaponMenu, VS::Input::Circle);
    if (Audio_Seq_unusedInventory.weapons[0].isEquipped == 0) {
      throw std::runtime_error(
          "real equipment-menu input did not equip the weapon");
    }
    VS::Items::SnapshotLiveGadgetBag();
  }
  void OpenEquippedContainerActions() {
    framework::ClearLiveMenuExcept(framework::kNoRow);
    if (Menu_Container_UpdateItemList(2) != 0) {
      throw std::runtime_error(
          "container list initializer unexpectedly completed");
    }
    RunFrames(Menu_Container_UpdateItemList);
    Frame(Menu_Container_UpdateItemList, VS::Input::Right);
    RunFrames(Menu_Container_UpdateItemList);
    Frame(Menu_Container_UpdateItemList, VS::Input::Circle);
    RunFrames(Menu_Container_UpdateItemList);
    Frame(Menu_Container_UpdateItemList, VS::Input::Circle);
    RunFrames(Menu_Container_UpdateItemList);
  }
};
}  // namespace container_dialog_live_tests
