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
#include "Battle/TargetingState.hpp"
#include "Content/DiscLayout.h"
#include "Core/Log.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/ItemRowsLiveState.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/Rename.hpp"
#include "Menu/RenameLiveState.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/EncodedText.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/UiPacketsLive.hpp"

// decomp: _renderRenameMenuStrings (src/MENU/MENU8.PRG/21A0.c)
std::uint16_t VS::Menu::Rename::renderRenameMenuStrings
    [VS::Menu::Rename::kMenuStringCount];

namespace VS::Menu::Rename {
namespace {
static_assert(sizeof(renderRenameMenuStrings) ==
              kMenuStringHalfwords * sizeof(std::uint16_t));
// vs_main_inventoryWeapon and vs_main_inventoryBlade (func_8006B57C_t.h).
constexpr std::size_t kEquippedOffset = 3;
constexpr std::size_t kWeaponNameOffset = 8;
static_assert(offsetof(VS::Items::InventoryWeapon, blade) == 1);
static_assert(offsetof(VS::Items::InventoryWeapon, is_equipped) ==
              kEquippedOffset);
static_assert(offsetof(VS::Items::InventoryWeapon, name) == kWeaponNameOffset);
static_assert(sizeof(VS::Items::InventoryWeapon::name) == kNameBytes);
static_assert(offsetof(VS::Items::InventoryBlade, id) == 0);

using Weapons = decltype(VS::Items::InventoryContents::weapons);
using Blades = decltype(VS::Items::InventoryContents::blades);
constexpr int kWeaponSlots = static_cast<int>(std::extent_v<Weapons>);
constexpr int kBladeSlots = static_cast<int>(std::extent_v<Blades>);
// ITEMNAME.BIN: one 24-byte name an item.
constexpr std::size_t kItemNames = VS_ITEMNAME_BIN_SIZE / kNameBytes;
constexpr int kItemNameCount = static_cast<int>(kItemNames);
// The information text box and vs_battle_menuState.currentMenu's weapon
// menu.
constexpr int kInformationTextBox = 7;
constexpr char kWeaponMenu = 13;

State renameState{};
// _nameEditorNameBuffer as the battle string context's first string: the
// confirmed name with a terminator after it.
std::array<std::uint8_t, kNameBytes + 1> promptName{};

class LiveHost final : public Host {
 public:
  void Refuse(std::string_view what) override {
    VS::Core::LogWarn("menu.rename", "{}", what);
  }
  std::span<const std::uint8_t> MenuStrings() override {
    return {reinterpret_cast<const std::uint8_t*>(renderRenameMenuStrings),
            sizeof renderRenameMenuStrings};
  }
  Framework::Buttons Input() override { return Framework::LiveButtons(); }
  void PlaySound(MenuSound sound) override { PlayMenuSound(sound); }
  void PlaySoundEffect(int effect) override {
    Audio::LivePlaySfxCentered(kMenuSoundChannelGroup, effect);
  }

  Render::PacketQueue& Queue(Framework::Layer layer) override {
    return queue_.emplace(Chain(layer));
  }
  void DrawGlyph(std::uint8_t glyph, int x, int y,
                 Framework::Layer layers) override {
    std::uint32_t* const chain = Chain(layers);
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
  void DrawButtonIcon(Framework::ButtonIcon button, int x, int y) override {
    Framework::DrawLiveButtonIcon(button, x, y, nullptr);
  }
  std::uint32_t GradientStopColor(std::uint32_t gradient,
                                  std::uint32_t color_index,
                                  int brightness) override {
    return static_cast<std::uint32_t>(
        Framework::LiveGradientStopColor(gradient, color_index, brightness));
  }
  int DrawCursor(int step, std::uint32_t position) override {
    return Framework::DrawLiveCursor(static_cast<std::uint32_t>(step),
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
    return VS::Menu::Framework::rowAnimationSteps;
  }

  void DismissInformationBox() override {
    Framework::DismissLiveInformationBox();
  }
  void InitializeInformationBox() override {
    Framework::InitializeLiveInformationBox();
  }
  void ShowConfirmPrompt(std::span<const std::uint8_t> name,
                         std::span<const std::uint8_t> prompt) override {
    VS::Menu::InitLiveInformationTextBox(InformationHeader::kCaution);
    promptName.fill(Render::kTextTerminator);
    std::memcpy(promptName.data(), name.data(),
                std::min(name.size(), kNameBytes));
    VS::Battle::stringContext.strings[0] =
        reinterpret_cast<char*>(promptName.data());
    if (VS::Menu::Framework::stringBuf == nullptr) {
      Refuse("rename prompt without the menu's string buffer");
      return;
    }
    const std::span<std::uint8_t> message(
        reinterpret_cast<std::uint8_t*>(VS::Menu::Framework::stringBuf),
        Framework::kStringBufferBytes);
    if (!Render::FormatEncodedText(message, prompt,
                                   Render::LiveEncodedTextArguments())) {
      Refuse("rename prompt does not fit the menu's string buffer");
      message[0] = Render::kTextTerminator;
    }
    Framework::SetLiveInformationMessage(VS::Menu::Framework::stringBuf);
  }
  void DismissConfirmPrompt() override {
    VS::Menu::DismissLiveTextBox(kInformationTextBox);
  }
  void ReturnToMenu() override {
    VS::Menu::Framework::menuState.control.current_menu = kWeaponMenu;
  }

  std::optional<Request> RenameRequest() override {
    if (VS::Menu::Framework::stringBuf == nullptr) {
      return std::nullopt;
    }
    return Request{
        .edit_existing = VS::Menu::Framework::stringBuf[0] == 1,
        .weapon = static_cast<std::uint8_t>(VS::Menu::Framework::stringBuf[1])};
  }
  std::optional<Weapon> LoadWeapon(int weapon) override {
    if (weapon < 0 || weapon >= kWeaponSlots) {
      return std::nullopt;
    }
    const VS::Items::InventoryWeapon& source =
        VS::Items::inventory.weapons[weapon];
    Weapon loaded{.blade = source.blade, .equipped = source.is_equipped != 0};
    std::memcpy(loaded.name.data(), source.name.data(), kNameBytes);
    return loaded;
  }
  std::optional<std::uint16_t> BladeItem(int blade) override {
    if (blade < 0 || blade >= kBladeSlots) {
      return std::nullopt;
    }
    return VS::Items::inventory.blades[blade].id;
  }
  std::span<const std::uint8_t> ItemName(int item) override {
    if (VS::Menu::Framework::itemNames == nullptr || item < 0 ||
        item >= kItemNameCount) {
      return {};
    }
    return {reinterpret_cast<const std::uint8_t*>(
                VS::Menu::Framework::itemNames[item]),
            kNameBytes};
  }
  void StoreWeaponName(int weapon,
                       std::span<const std::uint8_t> name) override {
    if (weapon < 0 || weapon >= kWeaponSlots) {
      Refuse("renamed weapon lies outside the inventory");
      return;
    }
    std::memcpy(VS::Items::inventory.weapons[weapon].name.data(), name.data(),
                std::min(name.size(), kNameBytes));
  }
  void EquipWeapon(int weapon) override {
    if (weapon < 0 || weapon >= kWeaponSlots) {
      Refuse("equipped weapon lies outside the inventory");
      return;
    }
    Items::EquipLiveAshleyWeapon(&VS::Items::inventory.weapons[weapon]);
  }

 private:
  // The ordering table entry a layer links into; null when it lies outside
  // the frame's table.
  [[nodiscard]] static std::uint32_t* Chain(Framework::Layer layer) {
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
  Context ctx{.state = renameState, .host = host};
  return Update(ctx, step);
}

void DrawLiveFrame(int x_offset) {
  LiveHost host;
  Context ctx{.state = renameState, .host = host};
  DrawFrame(ctx, x_offset);
}

void DrawLiveSelectionBox(const Box& box) {
  LiveHost host;
  Context ctx{.state = renameState, .host = host};
  DrawSelectionBox(ctx, box);
}

State& LiveRenameState() { return renameState; }

void ResetLiveRenameStatics() {
  // The statics with an initial value; the zero-initialised ones were not
  // cleared by a reload.
  renameState.nav_icon_pulse = false;
  renameState.nav_icon_animation = 0;
  renameState.selected_action = 0;
  renameState.action_counters = {};
  renameState.confirmation_cursor = 0;
  renameState.confirmed_name = {};
  promptName = {};
}
}  // namespace VS::Menu::Rename
