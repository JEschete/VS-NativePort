// Hand-maintained (tools/port/owned.csv).
#include "Menu/RenameLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include "Audio/SfxLive.hpp"
#include "Core/Log.hpp"
#include "Items/EquipmentLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Rename.hpp"
#include "Menu/Sounds.hpp"
#include "Render/EncodedText.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/TargetingState.h"
#include "Content/DiscLayout.h"
#include "Input/ControllerLiveState.h"
#include "Items/Inventory.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/RenameLiveState.h"
#include "Menu/TextBox.h"
}

// decomp: _renderRenameMenuStrings (src/MENU/MENU8.PRG/21A0.c)
u_short Menu_Rename_renderRenameMenuStrings[kMenuRenameMenuStringCount];

namespace VS::Menu::Rename {
namespace {
static_assert(sizeof(Menu_Rename_renderRenameMenuStrings) ==
              kMenuStringHalfwords * sizeof(std::uint16_t));
// vs_main_inventoryWeapon and vs_main_inventoryBlade (func_8006B57C_t.h).
constexpr std::size_t kEquippedOffset = 3;
constexpr std::size_t kWeaponNameOffset = 8;
static_assert(offsetof(Items_InventoryWeapon, blade) == 1);
static_assert(offsetof(Items_InventoryWeapon, isEquipped) == kEquippedOffset);
static_assert(offsetof(Items_InventoryWeapon, name) == kWeaponNameOffset);
static_assert(sizeof(Items_InventoryWeapon::name) == kNameBytes);
static_assert(offsetof(Items_InventoryBlade, id) == 0);

using Weapons = decltype(Items_Inventory::weapons);
using Blades = decltype(Items_Inventory::blades);
constexpr int kWeaponSlots = static_cast<int>(std::extent_v<Weapons>);
constexpr int kBladeSlots = static_cast<int>(std::extent_v<Blades>);
// ITEMNAME.BIN: one 24-byte name an item.
constexpr std::size_t kItemNames = VS_ITEMNAME_BIN_SIZE / kNameBytes;
constexpr int kItemNameCount = static_cast<int>(kItemNames);
// The information text box and vs_battle_menuState.currentMenu's weapon
// menu.
constexpr int kInformationTextBox = 7;
constexpr char kWeaponMenu = 13;

State g_state{};
// _nameEditorNameBuffer as the battle string context's first string: the
// confirmed name with a terminator after it.
std::array<std::uint8_t, kNameBytes + 1> g_prompt_name{};

class LiveHost final : public Host {
 public:
  void Refuse(std::string_view what) override {
    VS_LOG_WARN("menu.rename", "{}", what);
  }
  std::span<const std::uint8_t> MenuStrings() override {
    return {reinterpret_cast<const std::uint8_t*>(
                Menu_Rename_renderRenameMenuStrings),
            sizeof Menu_Rename_renderRenameMenuStrings};
  }
  Framework::Buttons Input() override {
    return {.pressed = Input_buttonsPressed.allButtons,
            .repeat = static_cast<std::uint32_t>(Input_buttonRepeat),
            .held = static_cast<std::uint32_t>(Input_buttonsState)};
  }
  void PlaySound(MenuSound sound) override { PlayMenuSound(sound); }
  void PlaySoundEffect(int effect) override {
    Audio::LivePlaySfxCentered(kMenuSoundChannelGroup, effect);
  }

  Render::PacketQueue& Queue(Framework::Layer layer) override {
    return queue_.emplace(Chain(layer));
  }
  void DrawGlyph(std::uint8_t glyph, int x, int y,
                 Framework::Layer layers) override {
    u_long* const chain = Chain(layers);
    if (chain == nullptr) {
      Refuse("rename glyph outside the frame's ordering table");
      return;
    }
    Render::DrawLiveVariableWidthGlyph(glyph, x, y, chain);
  }
  void SetFontStyle(int style) override { Render::SetLiveMenuFontStyle(style); }
  void DrawCompactText(std::string_view text, std::uint32_t position,
                       std::uint32_t color) override {
    const std::string terminated(text);
    Render::DrawLiveCompactTextColor(terminated.c_str(),
                                     static_cast<int>(position),
                                     static_cast<int>(color), nullptr);
  }
  void DrawButtonIcon(int button, int x, int y) override {
    Framework::DrawLiveButtonIcon(button, x, y, nullptr);
  }
  std::uint32_t GradientStopColor(std::uint32_t gradient,
                                  std::uint32_t color_index,
                                  int brightness) override {
    return static_cast<std::uint32_t>(
        Framework::LiveGradientStopColor(gradient, color_index, brightness));
  }
  int DrawCursor(int step, std::uint32_t position) override {
    return Framework::DrawLiveCursor(static_cast<u_int>(step),
                                     static_cast<int>(position));
  }
  int DrawMainMenuCursor(int step, int position) override {
    return Framework::DrawLiveMainMenuCursor(step, position);
  }

  bool RowsIdle() override { return Framework::AreLiveMenuRowsIdle() != 0; }
  Framework::MenuRow& SetMenuItem(const Framework::RowPlacement& placement,
                                  std::span<const std::uint8_t> text) override {
    return Framework::SetLiveMenuItemText(placement, text);
  }
  Framework::MenuRow& MenuItem(int row) override {
    return Framework::LiveMenuRow(row);
  }
  void FlyOutRowRight(int row) override { Framework::FlyOutLiveRowRight(row); }
  void FlyOutRowLeft(int row) override { Framework::FlyOutLiveRowLeft(row); }
  std::span<const std::uint8_t> RowAnimationSteps() override {
    return {
        reinterpret_cast<const std::uint8_t*>(Menu_Framework_rowAnimationSteps),
        sizeof Menu_Framework_rowAnimationSteps};
  }

  void DismissInformationBox() override {
    Framework::DismissLiveInformationBox();
  }
  void InitializeInformationBox() override {
    Framework::InitializeLiveInformationBox();
  }
  void ShowConfirmPrompt(std::span<const std::uint8_t> name,
                         std::span<const std::uint8_t> prompt) override {
    Menu_InitInformationTextBox(static_cast<enum Menu_InformationBoxHeader>(
        Framework::InformationHeader::kCaution));
    g_prompt_name.fill(Render::kTextTerminator);
    std::memcpy(g_prompt_name.data(), name.data(),
                std::min(name.size(), kNameBytes));
    Battle_stringContext.strings[0] =
        reinterpret_cast<char*>(g_prompt_name.data());
    if (Menu_Framework_stringBuf == nullptr) {
      Refuse("rename prompt without the menu's string buffer");
      return;
    }
    const std::span<std::uint8_t> message(
        reinterpret_cast<std::uint8_t*>(Menu_Framework_stringBuf),
        Framework::kStringBufferBytes);
    if (!Render::FormatEncodedText(message, prompt,
                                   Render::LiveEncodedTextArguments())) {
      Refuse("rename prompt does not fit the menu's string buffer");
      message[0] = Render::kTextTerminator;
    }
    Framework::SetLiveInformationMessage(Menu_Framework_stringBuf);
  }
  void DismissConfirmPrompt() override {
    Menu_DismissTextBox(kInformationTextBox);
  }
  void ReturnToMenu() override {
    Menu_Framework_menuState.currentMenu = kWeaponMenu;
  }

  std::optional<Request> RenameRequest() override {
    if (Menu_Framework_stringBuf == nullptr) {
      return std::nullopt;
    }
    return Request{
        .edit_existing = Menu_Framework_stringBuf[0] == 1,
        .weapon = static_cast<std::uint8_t>(Menu_Framework_stringBuf[1])};
  }
  std::optional<Weapon> LoadWeapon(int weapon) override {
    if (weapon < 0 || weapon >= kWeaponSlots) {
      return std::nullopt;
    }
    const Items_InventoryWeapon& source =
        Audio_Seq_unusedInventory.weapons[weapon];
    Weapon loaded{.blade = source.blade, .equipped = source.isEquipped != 0};
    std::memcpy(loaded.name.data(), source.name, kNameBytes);
    return loaded;
  }
  std::optional<std::uint16_t> BladeItem(int blade) override {
    if (blade < 0 || blade >= kBladeSlots) {
      return std::nullopt;
    }
    return Audio_Seq_unusedInventory.blades[blade].id;
  }
  std::span<const std::uint8_t> ItemName(int item) override {
    if (Menu_Framework_itemNames == nullptr || item < 0 ||
        item >= kItemNameCount) {
      return {};
    }
    return {
        reinterpret_cast<const std::uint8_t*>(Menu_Framework_itemNames[item]),
        kNameBytes};
  }
  void StoreWeaponName(int weapon,
                       std::span<const std::uint8_t> name) override {
    if (weapon < 0 || weapon >= kWeaponSlots) {
      Refuse("renamed weapon lies outside the inventory");
      return;
    }
    std::memcpy(Audio_Seq_unusedInventory.weapons[weapon].name, name.data(),
                std::min(name.size(), kNameBytes));
  }
  void EquipWeapon(int weapon) override {
    if (weapon < 0 || weapon >= kWeaponSlots) {
      Refuse("equipped weapon lies outside the inventory");
      return;
    }
    Items::EquipLiveAshleyWeapon(&Audio_Seq_unusedInventory.weapons[weapon]);
  }

 private:
  // The ordering table entry a layer links into; null when it lies outside
  // the frame's table.
  [[nodiscard]] static u_long* Chain(Framework::Layer layer) {
    switch (layer.base) {
      case Framework::LayerBase::kScene:
        return Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene,
                                              layer.slot);
      case Framework::LayerBase::kOverlay:
        return Render::LiveFrameOrderingEntry(
            Render::LiveOrderingBase::kOverlay, layer.slot);
      case Framework::LayerBase::kCaller:
        break;
    }
    return nullptr;
  }

  std::optional<Render::LivePacketQueue> queue_;
};
}  // namespace

int UpdateLiveRename(std::uint8_t& step) {
  LiveHost host;
  Context ctx{.state = g_state, .host = host};
  return Update(ctx, step);
}

void DrawLiveFrame(int x_offset) {
  LiveHost host;
  Context ctx{.state = g_state, .host = host};
  DrawFrame(ctx, x_offset);
}

void DrawLiveSelectionBox(const Box& box) {
  LiveHost host;
  Context ctx{.state = g_state, .host = host};
  DrawSelectionBox(ctx, box);
}

State& LiveRenameState() { return g_state; }

void ResetLiveRenameStatics() {
  // The statics with an initial value; the zero-initialised ones were not
  // cleared by a reload.
  g_state.nav_icon_pulse = false;
  g_state.nav_icon_animation = 0;
  g_state.selected_action = 0;
  g_state.action_counters = {};
  g_state.confirmation_cursor = 0;
  g_state.confirmed_name = {};
  g_prompt_name = {};
}
}  // namespace VS::Menu::Rename
