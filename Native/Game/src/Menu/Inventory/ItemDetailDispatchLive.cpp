#include "Menu/Inventory/ItemDetailDispatchLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Input/Pad.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Inventory/DetailsPoliciesLive.hpp"
#include "Menu/Inventory/EquipmentDetailControllersLive.hpp"
#include "Menu/Inventory/ItemDetailDispatch.hpp"
#include "Menu/Inventory/ItemDetailLiveSupport.hpp"
#include "Render/SpritesLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Actor.h"
#include "Core/Diagnostics.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Inventory/Inventory.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace VS::Menu::Inventory {
namespace {
using Category = Items::ItemCategory;
constexpr std::uint8_t kAccessoryCategory = 7;
constexpr std::uint32_t kSelectionRowShift = 8;
constexpr std::uint32_t kDispatchCategoryShift = 4;
constexpr std::uint32_t kDispatchCategoryMask = 0xF;
constexpr std::uint32_t kNonzeroSelectionMarker = 1U << 16;
constexpr std::int32_t kIconSize = 16;
constexpr std::uint32_t kFirstIconU = 120;
constexpr std::uint32_t kIconV = 128;
constexpr std::uint32_t kTextureVShift = 8;
constexpr std::uint32_t kSelectedIconPalette = 0x37FD0000;
constexpr std::uint32_t kIdleIconPalette = 0x37FE0000;
EquipmentDetailState g_armor_state;
EquipmentDetailState g_gem_state;
ItemDetailDispatcherState g_dispatcher;

[[noreturn]] void Refuse(const char* reason) {
  Core_Errorf("menu.inventory", "Item details refused: %s", reason);
  std::abort();
}

void RequireRows() {
  if (Menu_Framework_menuItems == nullptr) {
    Refuse("missing menu row workspace");
  }
}

void RequireRow(DetailRowId row) {
  RequireRows();
  if (row.index < 0 ||
      static_cast<std::size_t>(row.index) >= Framework::kRowCount) {
    Refuse("item detail row outside its menu workspace");
  }
}

Framework::MenuRow& Row(DetailRowId row) {
  RequireRow(row);
  return Framework::LiveMenuRow(row.index);
}

template <typename Record, std::size_t Count>
Record& Referenced(Record (&table)[Count], InventorySlot slot) {
  if (slot.one_based == 0 || slot.one_based > Count) {
    Refuse("item detail reference outside its inventory table");
  }
  return table[slot.one_based - 1];
}

template <typename Record, std::size_t Count>
void RequireBoundTable(const Record* bound, Record (&table)[Count]) {
  if (bound != table) {
    Refuse("item detail table is not the current inventory");
  }
}

void RequireFrame(bool overlay) {
  if (Render_frameScratch.primitiveCursor == nullptr ||
      (overlay ? Render_frameScratch.overlayOrderingTable
               : Render_frameScratch.orderingTable) == nullptr) {
    Refuse("missing current item detail presentation frame");
  }
}

std::optional<DetailSelection> Selection(int item_info) {
  if (item_info == 0) {
    return std::nullopt;
  }
  const auto command = static_cast<std::uint32_t>(item_info);
  return DetailSelection{
      DetailRowId{static_cast<std::uint8_t>(command >> kSelectionRowShift)},
      DetailPage{static_cast<std::uint8_t>(command - 1)}};
}

int ControllerCommand(std::optional<DetailSelection> selection) {
  if (!selection) {
    return 0;
  }
  const auto item = selection.value();
  const auto page =
      static_cast<std::uint8_t>(Core::Math::WrappingAdd(item.page.index, 1));
  const auto command =
      (static_cast<std::uint32_t>(static_cast<std::uint8_t>(item.row.index))
       << kSelectionRowShift) |
      page;
  // A nonzero original command can have zero in both stored entry bytes.
  return static_cast<int>(command != 0 ? command : kNonzeroSelectionMarker);
}

std::optional<DetailDispatchEntry> DispatchEntry(int item_info) {
  if (item_info == 0) {
    return std::nullopt;
  }
  return DetailDispatchEntry{
      static_cast<Category>(static_cast<std::uint32_t>(item_info) &
                            kDispatchCategoryMask),
      Selection(item_info >> kDispatchCategoryShift)};
}

std::span<char> TextView(char* text, std::span<char> borrowed_buffer) {
  if (text == nullptr) {
    Refuse("formatter did not supply a used item row text");
  }
  if (auto view = FindLiveItemDetailText(text, borrowed_buffer);
      !view.empty()) {
    return view;
  }
  Refuse("formatter item row text is outside its current owners");
}

void RequireRowText(bool buffer) {
  if (Menu_Framework_itemHelp == nullptr ||
      Menu_Framework_itemNames == nullptr) {
    Refuse("missing current item help or name text");
  }
  if (buffer && Menu_Framework_stringBuf == nullptr) {
    Refuse("missing current item description workspace");
  }
}

class LiveItemWorld final : public LiveDetailWorld<ItemDetailWorld> {
 public:
  explicit LiveItemWorld(Category category)
      : LiveDetailWorld(RequireRows), category_(category) {}

  InventorySlot CurrentItem(DetailPage page) override {
    const std::span<const std::uint8_t> order =
        category_ == Category::kArmor
            ? std::span<const std::uint8_t>{Audio_Seq_unusedInventoryIndices
                                                .armor}
            : std::span<const std::uint8_t>{
                  Audio_Seq_unusedInventoryIndices.gems};
    if (page.index < 0 ||
        static_cast<std::size_t>(page.index) >= order.size()) {
      Refuse("current item detail page outside its direct display order");
    }
    return InventorySlot{order[static_cast<std::size_t>(page.index)]};
  }
  void SetStatistics(InventorySlot slot) override {
    if (category_ == Category::kArmor) {
      if (slot.one_based != 0) {
        RequireBoundTable(Menu_Framework_armor,
                          Audio_Seq_unusedInventory.armor);
        const auto armor = Items::Live::Typed(
            Referenced(Audio_Seq_unusedInventory.armor, slot));
        if (armor.limb >= 1 && armor.limb <= Items::kLimbCount &&
            (Battle_characterState == nullptr ||
             Battle_characterState->stats == nullptr)) {
          Refuse("missing equipped limb armor statistics");
        }
      }
      Framework::SetLiveArmorStats(slot.one_based);
    } else {
      RequireBoundTable(Menu_Framework_gems, Audio_Seq_unusedInventory.gems);
      (void)Referenced(Audio_Seq_unusedInventory.gems, slot);
      Framework::SetLiveGemStats(slot.one_based);
    }
  }
  bool ArmorShowsDp(InventorySlot slot) override {
    return Items::Live::Typed(Referenced(Audio_Seq_unusedInventory.armor, slot))
               .category != kAccessoryCategory;
  }
  void OpenDpBar() override {
    Framework::UpdateLiveDpPpBars(Framework::dp_pp_bars::kDp);
  }
  void Exit(bool clear_dp_pp) override {
    ExitLiveDetailsView(clear_dp_pp ? 1 : 0);
  }
  void SetBattleReturnIntent(BattleReturnIntent intent) override {
    Menu_Inventory_exitToBattle =
        intent == BattleReturnIntent::kBattle ? Input::Triangle : 0;
  }
  DetailPage PollPaging(DetailPage page) override {
    return {PollLiveDetailsPaging(static_cast<int>(category_), page.index)};
  }
  InventorySlot SelectItem(DetailPage page) override {
    return {static_cast<std::uint8_t>(
        SelectLiveDetailsItem(static_cast<int>(category_), page.index))};
  }
  DetailRowView FormatItem(InventorySlot slot) override {
    std::array<char*, 2> text{};
    int row_type = 0;
    char* const buffer = Menu_Framework_stringBuf;
    const std::span<char> borrowed_buffer{
        buffer, buffer == nullptr ? 0 : Framework::kStringBufferBytes};
    if (category_ == Category::kArmor) {
      auto& record = Referenced(Audio_Seq_unusedInventory.armor, slot);
      const auto armor = Items::Live::Typed(record);
      if (armor.category == 0 || armor.category > kAccessoryCategory) {
        Refuse("used armor row category outside its help entries");
      }
      RequireRowText(armor.category != kAccessoryCategory);
      Framework::FormatLiveInventoryArmorRow(&record, text.data(), &row_type,
                                             buffer);
    } else {
      auto& record = Referenced(Audio_Seq_unusedInventory.gems, slot);
      RequireRowText(true);
      Framework::FormatLiveGemRow(&record, text.data(), &row_type, buffer);
    }
    return {.name = TextView(text[0], borrowed_buffer),
            .description = TextView(text[1], borrowed_buffer),
            .row_type = static_cast<std::uint32_t>(row_type)};
  }
  void ChangePage(DetailRowId row, const DetailRowView& text,
                  DetailPage page) override {
    RequireRow(row);
    std::array<char*, 2> borrowed{text.name.data(), text.description.data()};
    Framework::UpdateLiveItemDetailsSelection(row.index, borrowed.data(),
                                              text.row_type, page.index);
  }

 private:
  Category category_;
};

class LiveIconWorld final : public CategoryIconWorld {
 public:
  std::uint8_t CurrentCategory() override {
    return Menu_Framework_menuMemory.opaque3A.currentCategory;
  }
  void DrawListTabNavigation() override {
    RequireFrame(true);
    Framework::DrawLiveTabNavigation(0);
  }
  Framework::MenuRow& CategoryHeader() override {
    return Row(kCategoryHeaderRow);
  }
  void DrawIcon(CategoryIcon icon) override {
    RequireFrame(icon.overlay);
    const auto xy = static_cast<int>(Framework::PackXy(icon.x, icon.y));
    const auto size = static_cast<int>(Framework::PackXy(kIconSize, kIconSize));
    auto* packet =
        icon.overlay
            ? Render::QueueLiveDefaultSprite(size, xy)
            : Render::QueueLiveSpriteWithTexturePage(
                  Render::kDefaultSpriteBrightness, xy, size,
                  static_cast<u_long*>(Render_frameScratch.orderingTable));
    packet[Render::kSpriteTextureWord] =
        (kFirstIconU + icon.category * kIconSize) | (kIconV << kTextureVShift) |
        (icon.selected_palette ? kSelectedIconPalette : kIdleIconPalette);
  }
};

class LiveDispatcherWorld final : public ItemDetailDispatcherWorld {
 public:
  void SetHeaderPlacement(DetailHeaderPlacement placement) override {
    Menu_Inventory_selectedItemCategoryIconOnTop = static_cast<char>(placement);
  }
  void SetBattleReturnIntent(BattleReturnIntent intent) override {
    Menu_Inventory_exitToBattle =
        intent == BattleReturnIntent::kBattle ? Input::Triangle : 0;
  }
  void ResetDetailCursor() override { Framework::ResetLiveItemDetailCursor(); }
  Framework::MenuRow& CategoryHeader() override {
    return Row(kCategoryHeaderRow);
  }
  void SetCommand(DetailCommand command) override {
    Framework::SetLiveCommandLabel(command == DetailCommand::kMenu
                                       ? Menu_Framework_MenuCommand_Menu
                                       : Menu_Framework_MenuCommand_None);
  }
  std::int32_t RunController(
      Category category, std::optional<DetailSelection> selection) override {
    const int command = ControllerCommand(selection);
    switch (category) {
      case Category::kWeapon:
        return UpdateLiveWeaponDetails(command);
      case Category::kBlade:
        return UpdateLiveBladeDetails(command);
      case Category::kGrip:
        return UpdateLiveGripDetails(command);
      case Category::kShield:
        return UpdateLiveShieldDetails(command);
      case Category::kArmor:
        return UpdateLiveArmorDetails(command);
      case Category::kGem:
        return UpdateLiveGemDetails(command);
      default:
        Inventory::Refuse("detail dispatcher category outside its controllers");
    }
  }
  bool ReturnsToBattle() override { return Menu_Inventory_exitToBattle != 0; }
  void SetCategoryVisibility(std::uint8_t visibility) override {
    Menu_Inventory_itemCategoryIconsEnabled = static_cast<char>(visibility);
  }
  void SetCommandVisibility(std::uint8_t visibility) override {
    Menu_Inventory_statusCommandState = static_cast<char>(visibility);
  }
  void SetPreviewLayout(PreviewLayout layout) override {
    Menu_Inventory_ashleyRenderState = static_cast<int>(layout);
  }
  std::int32_t RowsReady() override {
    RequireRows();
    return Framework::AreLiveMenuRowsIdle();
  }
  std::uint8_t SelectedDetailElement() override {
    return Menu_Framework_selectedStatusViewElement;
  }
  void SetCursorBright(bool bright) override {
    Menu_Framework_cursorColor = bright ? 1 : 0;
  }
  void DrawDetailTabNavigation() override {
    RequireFrame(true);
    Framework::DrawLiveTabNavigation(1);
  }
  void DrawItemDetails() override {
    RequireRows();
    if (Menu_Framework_selectedStatusViewElement >=
        Framework::kStatusViewElementCount) {
      Inventory::Refuse("used detail cursor outside its elements");
    }
    Framework::UpdateLiveItemDetails();
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    Core_Errorf("menu.inventory", "Item details refused: %.*s",
                static_cast<int>(reason.size()), reason.data());
    std::abort();
  }
};
}  // namespace

// decomp: _renderItemCategory (src/MENU/items0.h:46-81).
void DrawLiveItemCategoryIcons(int animation_step, int highlight_selected) {
  LiveIconWorld world;
  DrawItemCategoryIcons(world, animation_step, highlight_selected);
}

// decomp: _armorStatusView (src/MENU/items1.h:464-544).
int UpdateLiveArmorDetails(int item_info) {
  LiveItemWorld world(Category::kArmor);
  return UpdateArmorDetails(g_armor_state, world, Selection(item_info));
}

// decomp: _gemStatusView (src/MENU/items1.h:554-630).
int UpdateLiveGemDetails(int item_info) {
  LiveItemWorld world(Category::kGem);
  return UpdateGemDetails(g_gem_state, world, Selection(item_info));
}

// decomp: _itemStatusDispatcher (src/MENU/items1.h:640-688).
int UpdateLiveItemDetails(int item_info) {
  LiveDispatcherWorld world;
  return UpdateItemDetails(g_dispatcher, world, DispatchEntry(item_info));
}

void ResetLiveItemDetailDispatch() {
  g_armor_state = {};
  g_gem_state = {};
  g_dispatcher = {};
}
}  // namespace VS::Menu::Inventory
