#include "Menu/Container/ContainerMenuDialogsLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>

#include "Audio/SfxLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Items/ContainerIndexingLive.hpp"
#include "Menu/Container/ContainerDetailHelpersLive.hpp"
#include "Menu/Container/ContainerMenuDialogs.hpp"
#include "Menu/Container/ContainerSortingLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRowsLiveHost.hpp"
#include "Menu/Sounds.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/TextBox.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace VS::Menu::container {
namespace {
constexpr std::size_t kMaximumActions = 24;
constexpr int kInformationBox = 7;
constexpr int kQuantityRow = 34;
constexpr int kQuantityY = 130;
constexpr int kQuantityWidth = 126;
constexpr int kQuantityFont = 5;
constexpr int kDiscardSound = 126;
constexpr int kDiscardSoundArgument = 28;
constexpr unsigned kCategoryShift = 12;
constexpr unsigned kKindShift = 16;
constexpr std::uint32_t kCategoryMask = 15;
constexpr char kTransferControls = 5;
SortMenuState g_sort;
DiscardMenuState g_discard;
DiscardPresentation g_presentation;
TransferConfirmationState g_transfer;

[[noreturn]] void Refuse(std::string_view reason) {
  Core_Errorf("menu.container", "Container dialog refused: %.*s",
              static_cast<int>(reason.size()), reason.data());
  std::abort();
}

void RequireRows() {
  if (Menu_Framework_menuItems == nullptr) {
    Refuse("missing current container dialog menu rows");
  }
}

void RequirePresentationFrame() {
  if (Render_frameScratch.primitiveCursor == nullptr ||
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kOverlay, 0) ==
          nullptr) {
    Refuse("missing current container dialog presentation frame");
  }
}

class LiveMenuDialogWorld final : public MenuDialogWorld,
                                  public ItemRowRestoreWorld {
 public:
  std::int32_t RowsReady() override {
    RequireRows();
    return Framework::AreLiveMenuRowsIdle();
  }
  void InitializeInformationBox() override {
    Menu_InitInformationTextBox(static_cast<Menu_InformationBoxHeader>(1));
  }
  bool InformationDone() override {
    return Menu_textBoxes[kInformationBox].flags.done != 0;
  }
  std::span<const std::uint8_t> HelpText() override {
    Framework::LiveItemRowsHost texts;
    return texts.HelpText();
  }
  std::span<const std::uint8_t> ContainerText() override {
    return {reinterpret_cast<const std::uint8_t*>(Menu_Container_menuText),
            sizeof Menu_Container_menuText};
  }
  void SetInformation(std::span<const std::uint8_t> text) override {
    Framework::SetLiveInformationMessageText(text);
  }
  void CreateActions(std::int32_t menu,
                     std::span<const Framework::RowText> rows) override {
    const std::array<std::int32_t, kMaximumActions> types{};
    if (rows.size() > types.size()) Refuse("too many container dialog actions");
    Framework::CreateLiveActionRowsText(menu, rows,
                                        std::span(types).first(rows.size()));
  }
  void UpdateActions() override {
    RequireRows();
    RequirePresentationFrame();
    Framework::UpdateLiveActionMenu();
  }
  std::int32_t ActionResult() override {
    return Framework::LiveActionSelectionResult();
  }
  void PlaySound(DialogSound sound) override {
    switch (sound) {
      case DialogSound::kSelect:
        Menu::PlaySelectSound();
        break;
      case DialogSound::kLeave:
        Menu::PlayLeaveSound();
        break;
      case DialogSound::kChange:
        Menu::PlayChangeSound();
        break;
      case DialogSound::kDiscard:
        Audio::LivePlaySfxCentered(kDiscardSound, kDiscardSoundArgument);
        break;
    }
  }
  void Sort(Items::ItemCategory category, std::int32_t stat) override {
    switch (category) {
      case Items::ItemCategory::kWeapon:
        SortLiveWeapons(stat);
        break;
      case Items::ItemCategory::kShield:
        SortLiveShields(stat);
        break;
      case Items::ItemCategory::kMisc:
        SortLiveMisc(stat);
        break;
      default:
        SortLiveEquipment(static_cast<int>(category), stat);
        break;
    }
  }
  Items::ContainerWorld& ActiveRecords() override {
    return active_.emplace(Menu_Container_containerData == nullptr
                               ? nullptr
                               : &Menu_Container_containerData->items);
  }
  Framework::MenuRow& QuantityRow(std::span<const std::uint8_t> text,
                                  std::int32_t x) override {
    RequireRows();
    return Framework::SetLiveMenuItemText(
        {.row = kQuantityRow, .x = x, .y = kQuantityY, .w = kQuantityWidth},
        text);
  }
  std::uint32_t PressedButtons() override {
    return Input_buttonsPressed.allButtons;
  }
  std::uint32_t RepeatedButtons() override { return Input_buttonRepeat; }
  void SetQuantityFont() override {
    Render::SetLiveMenuFontStyle(kQuantityFont);
  }
  void DrawQuantityDigit(std::uint32_t digit, std::int32_t x) override {
    RequirePresentationFrame();
    if (digit >= Render::LiveVariableFontGlyphWidths().size()) {
      Refuse("discard digit outside the current variable font");
    }
    auto* const foreground =
        Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kOverlay, 0);
    if (foreground == nullptr) Refuse("discard digit target outside the frame");
    // The original caller's -3 and font callee's +3 cancel. Borrow the
    // actual foreground target, never a pointer before its native owner.
    Render::LivePacketQueue queue(foreground);
    Render::DrawVariableWidthGlyph(queue, Render::LiveFontStyle(),
                                   Render::LiveVariableFontGlyphWidths(), digit,
                                   x, kQuantityY);
  }
  std::int32_t DrawQuantityCursor(std::uint8_t animation,
                                  std::int32_t position) override {
    RequirePresentationFrame();
    return Framework::DrawLiveCursor(animation, position);
  }
  std::uint8_t TransferDirection() override {
    return Menu_Container_browsingGadgetBag;
  }
  void PrepareTransferControls() override {
    Menu_Container_controlsPanelMode = kTransferControls;
    Menu_Inventory_freezeTabArrows = 0;
  }
  void FlyOutAction(std::int32_t row) override {
    RequireRows();
    Framework::FlyOutLiveRowLeft(row);
  }
  void ClearTransferRows() override {
    RequireRows();
    Framework::ClearLiveMenuExcept(0);
  }
  void HideTransferPanels() override { SetLivePanelVisibility(0); }
  std::uint8_t ListAnimationRow() override {
    return static_cast<std::uint8_t>(Menu_Framework_itemsListRow);
  }
  void AdvanceListRows() override {
    RequireRows();
    RequirePresentationFrame();
    Framework::UpdateLiveRowSelection();
  }
  Framework::MenuRow& Row(std::int32_t index) override {
    RequireRows();
    if (index < 0 || static_cast<std::size_t>(index) >= Framework::kRowCount) {
      Refuse("restored row outside the current container menu");
    }
    return Framework::LiveMenuRow(index);
  }
  std::span<const std::uint8_t> CursorMemory() override {
    return Framework::LiveMenuCursorMemory();
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    container::Refuse(reason);
  }

 private:
  std::optional<Items::LiveContainerWorld> active_;
};
}  // namespace

// decomp: _displaySortMenu (src/MENU/MENUD.PRG/234.c:1601), 80105ACC.
int UpdateLiveSortMenu(int category_selection) {
  LiveMenuDialogWorld world;
  const auto entry =
      category_selection == 0
          ? std::optional<Items::ItemCategory>{}
          : static_cast<Items::ItemCategory>(static_cast<std::uint8_t>(
                Core::Math::WrappingSubtract(category_selection, 1)));
  return UpdateSortMenu(g_sort, world, entry);
}

// decomp: _discardMenu (src/MENU/MENUD.PRG/234.c:1680), 80105D24.
int UpdateLiveDiscardMenu(int packed_selection) {
  LiveMenuDialogWorld world;
  std::optional<DiscardEntry> entry;
  if (packed_selection != 0) {
    const auto command = static_cast<std::uint32_t>(packed_selection);
    entry = DiscardEntry{static_cast<Items::ItemCategory>(
                             (command >> kCategoryShift) & kCategoryMask),
                         static_cast<std::uint8_t>(command - 1U),
                         static_cast<std::uint8_t>(command >> kKindShift)};
  }
  return UpdateDiscardMenu(g_discard, g_presentation, world, entry);
}

// decomp: func_801071D8 (src/MENU/MENUD.PRG/234.c:2309), 801071D8.
void RestoreLiveItemRows(int category) {
  LiveMenuDialogWorld world;
  RestoreItemRows(world, category);
}

// decomp: func_801072B0 (src/MENU/MENUD.PRG/234.c:2332), 801072B0.
int UpdateLiveTransferConfirmation(int initialize) {
  LiveMenuDialogWorld world;
  return UpdateTransferConfirmation(g_transfer, world, initialize != 0);
}

void ResetLiveContainerDialogStatics() { g_presentation = {}; }

void ResetLiveContainerDialogSession() {
  g_sort = {};
  g_discard = {};
  g_transfer = {};
  ResetLiveContainerDialogStatics();
}
}  // namespace VS::Menu::container
