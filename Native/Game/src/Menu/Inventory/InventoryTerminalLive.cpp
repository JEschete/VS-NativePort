#include "Menu/Inventory/InventoryTerminalLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Audio/SfxLive.hpp"
#include "Battle/ActionCostsLive.hpp"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Hud.h"
#include "Battle/HudStateLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/StringTables.h"
#include "Core/Diagnostics.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/InventoryLiveState.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Framework/ItemRowsLiveHost.hpp"
#include "Menu/Framework/ItemRowsLiveState.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Menu/Inventory/EquipmentMenusLive.hpp"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/InventorySortingLive.hpp"
#include "Menu/Inventory/InventoryTerminal.hpp"
#include "Menu/Inventory/ItemDetailDispatchLive.hpp"
#include "Menu/Inventory/ItemList.hpp"
#include "Menu/Inventory/ItemListRowsLive.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/TextBox.h"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/Render.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/StateFlags.hpp"

namespace VS::Menu::Inventory {
namespace {
using Category = TerminalCategory;
constexpr std::size_t kControllerCount = 5;
constexpr std::size_t kContextCount = 2;
constexpr std::size_t kInformationBox = 7;
constexpr std::size_t kLootTextModuleOffset = 0x7A80;
constexpr std::int32_t kQuantityFont = 5;
constexpr std::int32_t kQuantityY = 130;
constexpr std::int32_t kDiscardSound = 126;
constexpr std::int32_t kDiscardSoundArgument = 28;
constexpr std::uint32_t kTarnished = 0x80000;
constexpr std::uint32_t kByteMask = 255;
constexpr unsigned kCategoryShift = 8;
constexpr unsigned kKindShift = 16;
constexpr unsigned kDetailsShift = 4;
constexpr std::uint32_t kStatusColor = 0x202020;
constexpr std::uint32_t kShiftMask = 31;
constexpr std::int32_t kTextPairEntries = 2;
constexpr std::int32_t kStatusBackgroundX = 16;
constexpr std::int32_t kStatusBackgroundY = 38;
constexpr std::int32_t kStatusBackgroundWidth = 88;
constexpr std::int32_t kStatusBackgroundHeight = 10;
constexpr std::int32_t kStatusButtonX = 8;
constexpr std::int32_t kStatusButtonY = 36;
constexpr std::int32_t kStatusTextX = 28;
constexpr char kMiscTitle[] = "\x16\x12\x1C\x0C\xE7\0\0\xF8";

enum class Controller : std::size_t {
  kSort,
  kDiscard,
  kNavigation,
  kShortcut,
  kScreen
};
struct ContextState {
  InventorySortState sort;
  InventoryDiscardState discard;
  InventoryDiscardPresentation presentation;
  InventoryNavigationState navigation;
  std::array<bool, kControllerCount> active{};
};
std::array<ContextState, kContextCount> g_contexts;
MiscShortcutState g_shortcut;
InventoryScreenState g_screen;
LootInventoryText g_loot_text;
std::uint64_t g_refusals = 0;

[[noreturn]] void Refuse(std::string_view reason) {
  ++g_refusals;
  VS::Core::Errorf("menu.inventory", "Inventory terminal refused: %.*s",
                   static_cast<int>(reason.size()), reason.data());
  std::abort();
}
void RequireIdleContext(const ContextState& context) {
  if (std::ranges::find(context.active, true) != context.active.end()) {
    Refuse("reset during an active inventory terminal call");
  }
}
void RequireRows() {
  if (VS::Menu::Framework::menuItems == nullptr) {
    Refuse("missing menu row workspace");
  }
}
void RequireFrame() {
  if (VS::Render::FrameScratchpad().primitive_cursor == nullptr ||
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kOverlay, 0) ==
          nullptr) {
    Refuse("missing inventory presentation frame");
  }
}
std::span<const std::uint8_t> ByteView(std::span<const char> bytes) {
  return {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()};
}
template <typename T, std::size_t Count>
std::span<const std::uint8_t> ByteView(const T (&table)[Count]) {
  return {reinterpret_cast<const std::uint8_t*>(table), sizeof(table)};
}
std::span<const std::uint8_t> OffsetText(std::span<const std::uint8_t> table,
                                         std::size_t halfword) {
  const auto text = Framework::ItemHelpText(table, halfword);
  if (!text) Refuse("text offset outside its loaded asset");
  return table.subspan(text->offset);
}
std::span<const std::uint8_t> EntryText(std::span<const std::uint8_t> table,
                                        std::int32_t entry) {
  const auto text = Framework::ItemHelpEntry(table, entry);
  if (!text) Refuse("text entry outside its loaded asset");
  return table.subspan(text->offset);
}
std::size_t ContextIndex(ItemRowsContext context) {
  return context == ItemRowsContext::kItemMenu ? 0 : 1;
}
std::span<std::uint8_t> FixedOrder(Category category) {
  auto& indices = VS::Items::inventoryIndices;
  const std::array<std::span<std::uint8_t>, 7> tables{
      indices.weapons, indices.blades, indices.grips, indices.shields,
      indices.armor,   indices.gems,   indices.misc};
  const auto index = static_cast<std::size_t>(category);
  if (index >= tables.size()) Refuse("order category outside the inventory");
  return tables[index];
}

void ReadRuntime(TerminalRuntime& runtime) {
  runtime.cursor = VS::Menu::Framework::menuMemory.inventory_cursors;
  runtime.presentation = {
      static_cast<std::uint8_t>(Menu_Inventory_secondaryTitleEnabled),
      static_cast<std::uint8_t>(Menu_Inventory_itemCategoryIconsEnabled),
      static_cast<std::uint8_t>(Menu_Inventory_statusCommandState),
      static_cast<std::uint8_t>(Menu_Inventory_secondaryTitleAnimationStep),
      static_cast<std::uint8_t>(Menu_Inventory_itemCategoryIconsAnimationStep),
      static_cast<std::uint8_t>(Menu_Inventory_statusCommandAnimationStep),
      static_cast<std::uint8_t>(Menu_Inventory_selectedItemCategoryIconOnTop),
      Menu_Inventory_ashleyRenderState};
  const auto& control = VS::Menu::Framework::menuState.control;
  runtime.control.current_menu = control.current_menu;
  runtime.control.return_state = control.return_state;
  runtime.control.execute_ability_type = control.execute_ability_type;
  const auto& use = VS::Menu::Framework::actionSelection;
  runtime.use = {use.menu_status, use.selection_kind,
                 VS::Menu::Framework::ExecuteAbilityAction(use),
                 VS::Menu::Framework::ExecuteAbilityItem(use),
                 VS::Items::miscItemInvoked};
  runtime.cursor_memory_enabled =
      static_cast<std::uint8_t>(VS::Input::settings.cursor_memory);
  runtime.freeze_tabs = VS::Menu::Inventory::freezeTabArrows;
  runtime.restrictions = VS::Menu::Framework::actionRestrictions;
  runtime.hud_slide_offset = Battle_hudSlideOffset;
}
void WriteRuntime(const TerminalRuntime& runtime) {
  VS::Menu::Framework::menuMemory.inventory_cursors = runtime.cursor;
  const auto& panels = runtime.presentation;
  Menu_Inventory_secondaryTitleEnabled =
      static_cast<char>(panels.secondary_title);
  Menu_Inventory_itemCategoryIconsEnabled =
      static_cast<char>(panels.category_icons);
  Menu_Inventory_statusCommandState = static_cast<char>(panels.status_command);
  Menu_Inventory_secondaryTitleAnimationStep =
      static_cast<char>(panels.secondary_step);
  Menu_Inventory_itemCategoryIconsAnimationStep =
      static_cast<char>(panels.icons_step);
  Menu_Inventory_statusCommandAnimationStep =
      static_cast<char>(panels.status_step);
  Menu_Inventory_selectedItemCategoryIconOnTop =
      static_cast<char>(panels.icon_on_top);
  Menu_Inventory_ashleyRenderState = panels.ashley_layout;
  auto& control = VS::Menu::Framework::menuState.control;
  control.current_menu = runtime.control.current_menu;
  control.return_state = runtime.control.return_state;
  control.execute_ability_type = runtime.control.execute_ability_type;
  auto& use = VS::Menu::Framework::actionSelection;
  use.menu_status = runtime.use.command;
  use.selection_kind = runtime.use.kind;
  VS::Menu::Framework::SetExecuteAbility(use, runtime.use.action,
                                         runtime.use.item);
  VS::Items::miscItemInvoked = runtime.use.slot;
  VS::Input::settings.cursor_memory =
      static_cast<char>(runtime.cursor_memory_enabled);
  VS::Menu::Inventory::freezeTabArrows = runtime.freeze_tabs;
  VS::Menu::Framework::actionRestrictions = runtime.restrictions;
  Battle_hudSlideOffset = runtime.hud_slide_offset;
}

class CallGuard {
 public:
  explicit CallGuard(bool& active) : active_(active) {
    if (active_) Refuse("same inventory controller reentered before returning");
    active_ = true;
  }
  ~CallGuard() { active_ = false; }
  CallGuard(const CallGuard&) = delete;
  CallGuard& operator=(const CallGuard&) = delete;
  CallGuard(CallGuard&&) = delete;
  CallGuard& operator=(CallGuard&&) = delete;

 private:
  bool& active_;
};
struct RowStorage {
  Items::InventoryContents inventory;
  std::array<std::array<std::uint8_t, Framework::kRowDescriptionBufferBytes>,
             kTerminalRows>
      description{};
};

class LiveTerminalHost final : public InventoryTerminalHost,
                               public ItemRowsWorld {
 public:
  explicit LiveTerminalHost(ItemRowsContext context, bool shortcut = false)
      : context_(context), shortcut_(shortcut) {
    ReadRuntime(runtime_);
  }
  void Publish() { WriteRuntime(runtime_); }
  template <typename Function>
  auto Call(Function&& function) -> std::invoke_result_t<Function> {
    Publish();
    if constexpr (std::is_void_v<std::invoke_result_t<Function>>) {
      std::invoke(std::forward<Function>(function));
      ReadRuntime(runtime_);
    } else {
      std::invoke_result_t<Function> result =
          std::invoke(std::forward<Function>(function));
      ReadRuntime(runtime_);
      return result;
    }
  }
  TerminalRuntime& Runtime() override { return runtime_; }
  Input::PadState Buttons() override {
    return {.held = static_cast<std::uint32_t>(VS::Input::buttonsState),
            .pressed = VS::Input::buttonsPressed,
            .repeat = static_cast<std::uint32_t>(VS::Input::buttonRepeat),
            .sticks = {}};
  }
  bool RowsReady() override {
    RequireRows();
    return Call([] { return Framework::AreLiveMenuRowsIdle() != 0; });
  }
  bool InformationDone() override {
    return Menu_textBoxes[kInformationBox].flags.done != 0;
  }
  std::span<const std::uint8_t> Text(TerminalText text,
                                     std::int32_t index) override {
    Framework::LiveItemRowsHost texts;
    const auto help = texts.HelpText();
    const auto menu = context_ == ItemRowsContext::kItemMenu
                          ? ByteView(Menu_Inventory_menuText)
                          : LiveLootInventoryText();
    switch (text) {
      case TerminalText::kSortName:
        return EntryText(help, Text_Menu_ITEMHELP_BIN_Index_sortMenu +
                                   index * kTextPairEntries);
      case TerminalText::kSortDescription:
        return EntryText(help, Text_Menu_ITEMHELP_BIN_Index_sortMenuDesc +
                                   index * kTextPairEntries);
      case TerminalText::kYesNo:
        return EntryText(help, Text_Menu_ITEMHELP_BIN_Index_confirmYes + index);
      case TerminalText::kDiscardPrompt:
        return EntryText(help,
                         Text_Menu_ITEMHELP_BIN_Index_discardConfirm + index);
      case TerminalText::kQuantityPrompt:
        return OffsetText(help, Text_Menu_ITEMHELP_BIN_Offset_discardConfirm);
      case TerminalText::kNoItems:
        return OffsetText(help,
                          Text_Menu_ITEMHELP_BIN_Offset_noItemsInCategory);
      case TerminalText::kManage:
        return EntryText(
            menu, (context_ == ItemRowsContext::kItemMenu
                       ? static_cast<int>(Text_Menu3_menuText_Index_equip)
                       : static_cast<int>(Text_Menub_menuText_Index_equip)) +
                      index);
      case TerminalText::kTarnished:
        return OffsetText(menu, Text_Menu3_menuText_Offset_equipmentTarnished);
      case TerminalText::kUse:
        return OffsetText(help, Text_Menu_ITEMHELP_BIN_Offset_useItem);
      case TerminalText::kUseDescription:
        return OffsetText(help, Text_Menu_ITEMHELP_BIN_Offset_useItemDesc);
      case TerminalText::kCannotUse:
        return OffsetText(help, Text_Menu_ITEMHELP_BIN_Offset_cannotUse);
      case TerminalText::kInsufficientMp:
        return OffsetText(menu, Text_Menu3_menuText_Offset_insufficientMp);
      case TerminalText::kUseProhibited:
        return OffsetText(menu, Text_Menub_menuText_Offset_itemUseProhibited);
      case TerminalText::kSearch:
        return OffsetText(help, Text_Menu_ITEMHELP_BIN_Offset_searchItem);
      case TerminalText::kSearchDescription:
        return OffsetText(help, Text_Menu_ITEMHELP_BIN_Offset_searchItemDesc);
      case TerminalText::kInvalidSearch:
        return OffsetText(help, Text_Menu_ITEMHELP_BIN_Offset_invalidSearch);
      case TerminalText::kDiscard:
        return OffsetText(help, Text_Menu_ITEMHELP_BIN_Offset_discardItem);
      case TerminalText::kDiscardDescription:
        return OffsetText(help, Text_Menu_ITEMHELP_BIN_Offset_discardItemDesc);
      case TerminalText::kDiscardRestriction:
        return EntryText(
            help,
            Text_Menu_ITEMHELP_BIN_Index_cannotDiscardEquipped - 1 + index);
      case TerminalText::kDiscardAll:
        return OffsetText(help, Text_Menu_ITEMHELP_BIN_Offset_discardAll);
      case TerminalText::kDiscardAllDescription:
        return OffsetText(help, Text_Menu_ITEMHELP_BIN_Offset_discardAllDesc);
      case TerminalText::kCannotDiscard:
        return OffsetText(help, Text_Menu_ITEMHELP_BIN_Offset_cannotDiscard);
      case TerminalText::kSort:
        return OffsetText(help, Text_Menu_ITEMHELP_BIN_Offset_sortItem);
      case TerminalText::kSortHint:
        return OffsetText(help, Text_Menu_ITEMHELP_BIN_Offset_sortItemDesc);
      case TerminalText::kItemsTitle:
        return EntryText(ByteView(VS::Menu::menuStrings),
                         Text_Battle_menuStrings_Index_items);
      case TerminalText::kMiscTitle:
        return ByteView(kMiscTitle);
      case TerminalText::kCategoryTitle:
        return EntryText(
            menu, (context_ == ItemRowsContext::kItemMenu
                       ? static_cast<int>(Text_Menu3_menuText_Index_weapons)
                       : static_cast<int>(Text_Menub_menuText_Index_weapons)) +
                      index);
    }
    Refuse("unknown inventory text selector");
  }
  void InitializeInformation() override {
    Call([] { Framework::InitializeLiveInformationBox(); });
  }
  void InitializeDiscardInformation() override {
    Call([] {
      Menu::InitLiveInformationTextBox(
          static_cast<Menu_InformationBoxHeader>(1));
    });
  }
  void DismissInformation() override {
    Call([] { Framework::DismissLiveInformationBox(); });
  }
  void SetInformation(std::span<const std::uint8_t> text) override {
    Call([&] { Framework::SetLiveInformationMessageText(text); });
  }
  void PlaySound(TerminalSound sound) override {
    Call([&] {
      switch (sound) {
        case TerminalSound::kSelect:
          Menu::PlaySelectSound();
          break;
        case TerminalSound::kLeave:
          Menu::PlayLeaveSound();
          break;
        case TerminalSound::kChange:
          Menu::PlayChangeSound();
          break;
        case TerminalSound::kInvalid:
          Menu::PlayInvalidSound();
          break;
        case TerminalSound::kDiscard:
          Audio::LivePlaySfxCentered(kDiscardSound, kDiscardSoundArgument);
          break;
      }
    });
  }
  void UpdateActions() override {
    RequireRows();
    RequireFrame();
    Call([] { Framework::UpdateLiveActionMenu(); });
  }
  std::int32_t ActionResult() override {
    return Call([] { return Framework::LiveActionSelectionResult(); });
  }
  void CreateActions(std::int32_t menu,
                     std::span<const Framework::RowText> rows,
                     std::span<const std::int32_t> flags) override {
    RequireRows();
    if (rows.size() != flags.size()) Refuse("action text/type extent mismatch");
    Call([&] { Framework::CreateLiveActionRowsText(menu, rows, flags); });
  }
  void UpdateActionInformation() override {
    Call([] {
      Framework::UpdateLiveInformationVisibility(
          Framework::LiveHighlightedActionRow(),
          VS::Menu::Framework::actionMenuState);
    });
  }
  void Sort(Category category, std::int32_t stat) override {
    Call([&] {
      switch (category) {
        case Category::kWeapon:
          SortLiveWeapons(stat);
          break;
        case Category::kShield:
          SortLiveShields(stat);
          break;
        case Category::kMisc:
          SortLiveMisc(stat);
          break;
        default:
          SortLiveEquipment(static_cast<int>(category), stat);
          break;
      }
    });
  }
  Framework::MenuRow& Row(std::int32_t row) override {
    RequireRows();
    if (row < 0 || static_cast<std::size_t>(row) >= Framework::kRowCount) {
      Refuse("row outside the active menu table");
    }
    return Framework::LiveMenuRow(row);
  }
  Framework::MenuRow& SetRow(const Framework::RowPlacement& placement,
                             std::span<const std::uint8_t> text) override {
    static_cast<void>(Row(placement.row));
    return Call([&]() -> Framework::MenuRow& {
      return Framework::SetLiveMenuItemText(placement, text);
    });
  }
  void ClearExcept(std::int32_t row) override {
    RequireRows();
    Call([&] { Framework::ClearLiveMenuExcept(row); });
  }
  void FlyOutExcept(std::int32_t row) override {
    RequireRows();
    Call([&] { Framework::FlyOutLiveRowsLeftExcept(row); });
  }
  void SetQuantityFont() override {
    Render::SetLiveMenuFontStyle(kQuantityFont);
  }
  void DrawQuantityDigit(std::uint32_t digit, std::int32_t x) override {
    RequireFrame();
    const auto widths = Render::LiveVariableFontGlyphWidths();
    if (digit >= widths.size()) Refuse("quantity digit outside the font");
    auto* foreground =
        Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kOverlay, 0);
    if (foreground == nullptr) Refuse("missing quantity foreground target");
    Render::LivePacketQueue queue(foreground);
    Render::DrawVariableWidthGlyph(queue, Render::LiveFontStyle(), widths,
                                   digit, x, kQuantityY);
  }
  std::int32_t DrawQuantityCursor(std::uint8_t animation, std::int32_t x,
                                  std::int32_t y) override {
    RequireFrame();
    return Call([&] {
      return Framework::DrawLiveCursor(
          animation, static_cast<int>(Framework::PackXy(x, y)));
    });
  }
  Items::InventoryContents Inventory() override { return VS::Items::inventory; }
  std::span<std::uint8_t> Order(Category category) override {
    if (shortcut_) return FixedOrder(category);
    const auto index = static_cast<std::size_t>(category);
    if (index >= static_cast<std::size_t>(Category::kManage)) {
      Refuse("inventory order category outside bound tables");
    }
    auto* pointer = VS::Menu::Inventory::categoryOrders[index];
    for (std::size_t table = 0;
         table < static_cast<std::size_t>(Category::kManage); ++table) {
      const auto entries = FixedOrder(static_cast<Category>(table));
      for (std::size_t offset = 0; offset < entries.size(); ++offset) {
        if (pointer == &entries[offset]) return entries.subspan(offset);
      }
    }
    Refuse("inventory display order has no current owner");
  }
  void SetMiscCount(std::size_t slot, std::uint8_t count) override {
    auto items = std::span(VS::Items::inventory.misc);
    if (slot >= items.size()) Refuse("misc write outside the inventory");
    items[slot].count = static_cast<char>(count);
  }
  void ClearItem(Category category, std::uint8_t slot) override {
    Call([&] {
      Items::Inventory::ClearLiveItemSlot(static_cast<int>(category), slot);
    });
  }
  void RebuildInventory(Category category) override {
    Call([&] {
      Items::Inventory::RebuildLiveDisplayIndices(static_cast<int>(category));
    });
  }
  std::int32_t ParentWeapon(Category category, std::int32_t slot) override {
    const auto parent =
        ParentWeaponIndex(static_cast<int>(category), slot, Inventory());
    if (!parent) Refuse("parent weapon read outside inventory");
    return *parent;
  }
  std::int32_t ParentShield(Category category, std::int32_t slot) override {
    const auto parent =
        ParentShieldIndex(static_cast<int>(category), slot, Inventory());
    if (!parent) Refuse("parent shield read outside inventory");
    return *parent;
  }
  std::int32_t FindItem(Category category, std::int32_t slot) override {
    return Call([&] {
      return Items::Inventory::FindLiveDisplayPosition(
          static_cast<int>(category), slot);
    });
  }
  std::uint8_t MiscAction(std::size_t row) override {
    const auto actions = std::span(Menu_Inventory_unusedMiscActions);
    if (row >= actions.size()) Refuse("misc action row outside its table");
    return static_cast<std::uint8_t>(actions[row]);
  }
  std::span<const std::uint8_t> MiscActionMap() override {
    return {Menu_Inventory_miscItemToActionMap, kMiscActionMapEntries};
  }
  bool ActionUnlocked(std::uint32_t action) override {
    const auto actions = std::span(Battle_actionDefinitions);
    if (action >= actions.size()) {
      Refuse("misc action outside battle definitions");
    }
    return actions[action].unlocked != 0;
  }
  std::int32_t ActionFlags(std::uint8_t action) override {
    if (Battle_characterState == nullptr ||
        Battle_characterState->stats == nullptr) {
      Refuse("action flags without Ashley or a valid action");
    }
    return Call(
        [&] { return Battle::LiveActorActionRestrictionFlags(0, action); });
  }
  std::uint8_t IntroState() override {
    return VS::State::stateFlags.intro_state;
  }
  bool EquipmentTarnished() override {
    if (Battle_characterState == nullptr ||
        Battle_characterState->stats == nullptr) {
      Refuse("Manage menu without Ashley's equipment state");
    }
    return (Battle_characterState->stats->statuses & kTarnished) != 0;
  }
  void BeginRows() override {
    if (storage_) Refuse("native row workspace already in use");
    storage_ = std::make_unique<RowStorage>();
  }
  void EndRows() noexcept override {
    active_rows_ = nullptr;
    storage_.reset();
  }
  std::int32_t PopulateRows(ItemRowsContext context, Category category,
                            TerminalRows& rows) override {
    if (!storage_) Refuse("row population without a native workspace");
    storage_->inventory = Inventory();
    active_rows_ = &rows;
    const auto result = BuildItemRows({.category = category,
                                       .context = context,
                                       .order = Order(category),
                                       .capacity = Capacity(category),
                                       .misc_action_map = MiscActionMap(),
                                       .row_capacity = kTerminalRows},
                                      storage_->inventory, *this);
    if (!result) {
      Refuse("inventory row builder refused its records or action map");
    }
    return *result;
  }
  void FormatMisc(std::size_t slot, std::size_t row,
                  TerminalRows& rows) override {
    if (!storage_) Refuse("misc formatting without a native workspace");
    storage_->inventory = Inventory();
    active_rows_ = &rows;
    FormatRow(Category::kMisc, slot, row);
  }
  void FormatRow(Category category, std::size_t slot,
                 std::size_t row) override {
    if (!storage_ || active_rows_ == nullptr || row >= kTerminalRows ||
        static_cast<std::size_t>(category) >=
            static_cast<std::size_t>(Category::kManage) ||
        slot >=
            Items::kCategoryCapacities[static_cast<std::size_t>(category)]) {
      Refuse("formatting outside native row storage or inventory");
    }
    Framework::LiveItemRowsHost texts;
    auto& inventory = storage_->inventory;
    auto& buffer = storage_->description[row];
    Framework::ItemRowText formatted;
    switch (category) {
      case Category::kWeapon:
        formatted = Framework::FormatInventoryWeaponRow(
            texts, inventory.weapons[slot], buffer);
        break;
      case Category::kBlade:
        formatted =
            Framework::FormatBladeRow(texts, inventory.blades[slot], buffer);
        break;
      case Category::kGrip:
        formatted =
            Framework::FormatGripRow(texts, inventory.grips[slot], buffer);
        break;
      case Category::kShield:
        formatted = Framework::FormatInventoryShieldRow(
            texts, inventory.shields[slot], buffer);
        break;
      case Category::kArmor:
        formatted = Framework::FormatInventoryArmorRow(
            texts, inventory.armor[slot], buffer);
        break;
      case Category::kGem:
        formatted =
            Framework::FormatGemRow(texts, inventory.gems[slot], buffer);
        break;
      case Category::kMisc:
        formatted =
            Framework::FormatMiscItemRow(texts, inventory.misc[slot], buffer);
        break;
      case Category::kManage:
        Refuse("Manage is not an inventory formatter");
    }
    if (!formatted.name || !formatted.description) {
      Refuse("formatter omitted a meaningful row text");
    }
    active_rows_->text[row] = {ResolveText(*formatted.name, row, slot),
                               ResolveText(*formatted.description, row, slot)};
    active_rows_->flags[row] = static_cast<std::int32_t>(formatted.row_type);
  }
  void AddRowFlags(std::size_t row, std::uint32_t flags) override {
    if (active_rows_ == nullptr || row >= kTerminalRows) {
      Refuse("row flag write outside workspace");
    }
    active_rows_->flags[row] = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(active_rows_->flags[row]) | flags);
  }
  void StoreMiscAction(std::size_t row, std::uint8_t action) override {
    auto actions = std::span(Menu_Inventory_unusedMiscActions);
    if (row >= actions.size()) Refuse("misc action write outside row table");
    actions[row] = static_cast<char>(action);
  }
  void CreateList(std::int32_t menu, std::span<const Framework::RowText> rows,
                  std::span<const std::int32_t> flags) override {
    RequireRows();
    if (rows.size() > kTerminalRows || rows.size() != flags.size()) {
      Refuse("list text/type extent mismatch");
    }
    std::array<std::array<std::uint8_t, Framework::kRowNameCopyBytes>,
               kTerminalRows>
        names{};
    std::array<std::array<std::uint8_t, Framework::kRowDescriptionCopyBytes>,
               kTerminalRows>
        descriptions{};
    std::array<char*, kTerminalRows * 2> text{};
    std::array<int, kTerminalRows> types{};
    for (std::size_t row = 0; row < rows.size(); ++row) {
      names[row].fill(Render::kTextTerminator);
      descriptions[row].fill(Render::kTextTerminator);
      std::ranges::copy(rows[row].name.first(
                            std::min(rows[row].name.size(), names[row].size())),
                        names[row].begin());
      std::ranges::copy(
          rows[row].description.first(
              std::min(rows[row].description.size(), descriptions[row].size())),
          descriptions[row].begin());
      text[row * 2] = reinterpret_cast<char*>(names[row].data());
      text[row * 2 + 1] = reinterpret_cast<char*>(descriptions[row].data());
      types[row] = flags[row];
    }
    Call([&] {
      Framework::CreateLiveMenuRows(static_cast<int>(rows.size()), menu,
                                    text.data(), types.data());
    });
  }
  void RestoreRows(Category category) override {
    RequireRows();
    Call([&] { LiveRestoreItemRows(static_cast<int>(category)); });
  }
  std::int32_t SelectedRow() override {
    RequireRows();
    return Call([] { return Framework::LiveSelectedRow(); });
  }
  std::int32_t StoreCursor() override {
    RequireRows();
    return Call([] { return Framework::CloseLiveRowsAndRememberCursor(); });
  }
  std::int32_t Details(std::optional<ItemDetailRequest> request) override {
    std::uint32_t packed = 0;
    if (request) {
      packed = ((static_cast<std::uint32_t>(request->stored_cursor) + 1U)
                << kDetailsShift) |
               static_cast<std::uint32_t>(request->category);
    }
    return Call(
        [&] { return UpdateLiveItemDetails(static_cast<int>(packed)); });
  }
  std::int32_t Discard(std::optional<InventoryDiscardEntry> entry) override {
    return Call([&] {
      auto& state = g_contexts[ContextIndex(context_)];
      const CallGuard guard(
          state.active[static_cast<std::size_t>(Controller::kDiscard)]);
      LiveTerminalHost host(context_);
      const auto result =
          UpdateDiscardMenu(state.discard, state.presentation, host, entry);
      host.Publish();
      return result;
    });
  }
  std::int32_t SortMenu(std::optional<Category> category) override {
    return Call([&] {
      auto& state = g_contexts[ContextIndex(context_)];
      const CallGuard guard(
          state.active[static_cast<std::size_t>(Controller::kSort)]);
      LiveTerminalHost host(context_);
      const auto result = UpdateSortMenu(state.sort, host, category);
      host.Publish();
      return result;
    });
  }
  bool ItemNamesLoaded() override {
    return VS::Menu::Framework::itemNames != nullptr;
  }
  bool LoadItemText(bool initialize) override {
    return Call(
        [&] { return Framework::LoadLiveItemText(initialize ? 1 : 0) != 0; });
  }
  bool UnloadItemText() override {
    return Call([] { return Framework::UnloadLiveItemTextWhenReady() != 0; });
  }
  void RenderScreen() override {
    RequireRows();
    RequireFrame();
    Call([] { Framework::UpdateLiveScreen(); });
  }
  void PopulateBaseStats(std::int32_t actor) override {
    Call([&] { Framework::SetLiveActorComparisonStats(actor); });
  }
  void SetFieldRenderingEnabled(bool enabled) override {
    Call([&] { Field::LiveSetFieldRenderingEnabled(enabled ? 1 : 0); });
  }
  void SetHudHidden(bool hidden) override {
    Call([&] { Battle::SetLiveHudHidden(hidden ? 1 : 0); });
  }
  void RenderAshley(Framework::AshleyRenderMode mode) override {
    if (mode == Framework::AshleyRenderMode::kRender) RequireFrame();
    Call([&] { Framework::RenderLiveAshley(mode); });
  }
  std::int32_t Navigation(std::int32_t initialize) override {
    return Call([&] { return UpdateLiveItemList(initialize); });
  }
  std::int32_t EquipmentMenu(bool initialize) override {
    return Call([&] { return UpdateLiveEquipmentMenu(initialize ? 1 : 0); });
  }
  void SetBackgroundPriority(std::int32_t offset,
                             std::int32_t brightness) override {
    Call([&] { Framework::SetLiveBackgroundPriority(offset, brightness); });
  }
  void CloseBackground() override {
    Call([] { Framework::SetLiveBackgroundFade(0); });
  }
  void ClearCommand() override {
    Call([] { Framework::SetLiveCommandLabel(Framework::MenuCommand::kNone); });
  }
  std::int32_t BackgroundFadeStep() override {
    return VS::Menu::Framework::backgroundFadeStep;
  }
  std::int32_t AnimationOffset(std::size_t step) override {
    const auto offsets = std::span(VS::Menu::Framework::rowAnimationSteps);
    if (step >= offsets.size()) Refuse("panel animation outside its curve");
    return static_cast<std::uint8_t>(offsets[step]);
  }
  std::uint8_t Capacity(Category category) override {
    const auto index = static_cast<std::size_t>(category);
    const auto capacity = std::span(VS::Items::kCategoryCapacities);
    if (index >= capacity.size()) {
      Refuse("capacity outside inventory categories");
    }
    return capacity[index];
  }
  std::int32_t CountItems(Category category) override {
    return Call([&] {
      return Items::Inventory::CountLiveItems(static_cast<int>(category),
                                              nullptr);
    });
  }
  void DrawCategoryIcons(std::uint8_t step, std::uint8_t on_top) override {
    RequireFrame();
    Call([&] { DrawLiveItemCategoryIcons(step, on_top); });
  }
  void DrawStatusCommand(std::int32_t offset, std::uint8_t state) override {
    RequireFrame();
    Call([&] {
      Framework::DrawLiveButtonPromptBackground(
          kStatusBackgroundX - offset, kStatusBackgroundY,
          kStatusBackgroundWidth, kStatusBackgroundHeight);
      Framework::DrawLiveButtonIcon(Framework::ButtonIcon::kSquare,
                                    kStatusButtonX - offset, kStatusButtonY,
                                    nullptr);
      Render::DrawLiveCompactTextColor(
          "STATUS",
          static_cast<int>(
              Framework::PackXy(kStatusTextX - offset, kStatusBackgroundY)),
          static_cast<int>(kStatusColor << (state & kShiftMask)), nullptr);
    });
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    Inventory::Refuse(reason);
  }

 private:
  std::span<const std::uint8_t> ResolveText(Framework::TextRef reference,
                                            std::size_t row, std::size_t slot) {
    if (!storage_) Refuse("text resolve without row storage");
    Framework::LiveItemRowsHost texts;
    std::span<const std::uint8_t> bytes;
    switch (reference.source) {
      case Framework::TextSource::kNone:
        return {};
      case Framework::TextSource::kEmpty: {
        static constexpr std::array<std::uint8_t, 1> kEmpty{
            Render::kTextTerminator};
        return kEmpty;
      }
      case Framework::TextSource::kBuffer:
        bytes = storage_->description[row];
        break;
      case Framework::TextSource::kWeaponName:
        if (slot >= storage_->inventory.weapons.size()) {
          Refuse("weapon name outside inventory");
        }
        bytes = ByteView(
            std::span<const char>(storage_->inventory.weapons[slot].name));
        break;
      case Framework::TextSource::kHelp:
        bytes = texts.HelpText();
        break;
      case Framework::TextSource::kNames:
        bytes = texts.NameText();
        break;
    }
    if (reference.offset >= bytes.size()) {
      Refuse("formatter text outside its owner");
    }
    return bytes.subspan(reference.offset);
  }
  ItemRowsContext context_;
  bool shortcut_;
  TerminalRuntime runtime_;
  std::unique_ptr<RowStorage> storage_;
  TerminalRows* active_rows_ = nullptr;
};

std::optional<Category> SortEntry(int initialize) {
  if (initialize == 0) return std::nullopt;
  return static_cast<Category>(
      static_cast<std::uint8_t>(static_cast<std::uint32_t>(initialize) - 1U));
}
std::optional<InventoryDiscardEntry> DiscardEntry(int initialize) {
  if (initialize == 0) return std::nullopt;
  const auto packed = static_cast<std::uint32_t>(initialize);
  return InventoryDiscardEntry{
      static_cast<Category>((packed >> kCategoryShift) & kByteMask),
      static_cast<std::uint8_t>(packed),
      static_cast<std::uint8_t>(packed >> kKindShift)};
}
template <typename Function>
int Execute(ItemRowsContext context, Controller controller,
            Function&& function) {
  auto& state = g_contexts[ContextIndex(context)];
  const CallGuard guard(state.active[static_cast<std::size_t>(controller)]);
  try {
    LiveTerminalHost host(context, controller == Controller::kShortcut);
    const int result = function(host, state);
    host.Publish();
    return result;
  } catch (const std::bad_alloc&) {
    Refuse("inventory terminal allocation failed");
  }
}
int SortLive(ItemRowsContext context, int initialize) {
  return Execute(context, Controller::kSort, [&](auto& host, auto& state) {
    return UpdateSortMenu(state.sort, host, SortEntry(initialize));
  });
}
int DiscardLive(ItemRowsContext context, int initialize) {
  return Execute(context, Controller::kDiscard, [&](auto& host, auto& state) {
    return UpdateDiscardMenu(state.discard, state.presentation, host,
                             DiscardEntry(initialize));
  });
}
int NavigateLive(ItemRowsContext context, int initialize) {
  return Execute(
      context, Controller::kNavigation, [&](auto& host, auto& state) {
        return UpdateItemList(state.navigation, host, context, initialize);
      });
}
}  // namespace

void BindLiveLootInventoryText(std::span<const std::uint8_t> bytes) {
  if (!BindLootInventoryText(g_loot_text, bytes)) {
    Refuse("MENUB text table has an invalid extent, offset or terminator");
  }
}
std::span<const std::uint8_t> LiveLootInventoryText() {
  if (!g_loot_text.loaded) {
    try {
      const auto* store = Content::DataStore::Get();
      const auto* entry = store ? store->Find("MENU/MENUB.PRG") : nullptr;
      if (store == nullptr || entry == nullptr) {
        Refuse("MENUB text has no extracted module source");
      }
      std::array<std::uint8_t, kLootInventoryTextBytes> bytes{};
      if (!store->Read(*entry, kLootTextModuleOffset, bytes)) {
        Refuse("MENUB text could not be read from its module");
      }
      BindLiveLootInventoryText(bytes);
    } catch (const std::bad_alloc&) {
      Refuse("MENUB text loading allocation failed");
    }
  }
  return g_loot_text.bytes;
}

// decomp: _sortItems (src/MENU/items1.h:1077), separately compiled in both
// overlays.
int UpdateLiveSortMenu(int initialize) {
  return SortLive(ItemRowsContext::kItemMenu, initialize);
}
int UpdateLiveLootSortMenu(int initialize) {
  return SortLive(ItemRowsContext::kLoot, initialize);
}
// decomp: _discardMenu (src/MENU/items1.h:1185).
int UpdateLiveDiscardMenu(int initialize) {
  return DiscardLive(ItemRowsContext::kItemMenu, initialize);
}
int UpdateLiveLootDiscardMenu(int initialize) {
  return DiscardLive(ItemRowsContext::kLoot, initialize);
}
// decomp: _itemNavigation (src/MENU/items2.h:201), MENU3 _ITEMMENU versus
// MENUB.
int UpdateLiveItemList(int initialize) {
  return NavigateLive(ItemRowsContext::kItemMenu, initialize);
}
int UpdateLiveLootItemList(int initialize) {
  return NavigateLive(ItemRowsContext::kLoot, initialize);
}
// decomp: vs_mainMenu_miscItemsShortcutMenu (src/MENU/MAINMENU.PRG/C48.c:480).
void UpdateLiveMiscItemShortcut(int initialize) {
  static_cast<void>(Execute(ItemRowsContext::kItemMenu, Controller::kShortcut,
                            [&](auto& host, auto&) {
                              UpdateMiscItemShortcut(g_shortcut, host,
                                                     initialize);
                              return 0;
                            }));
}
// decomp: vs_menu3_exec (src/MENU/MENU3.PRG/16C.c:1236).
int UpdateLiveInventoryScreen(unsigned char* step) {
  if (step == nullptr) Refuse("inventory screen without a menu state");
  return Execute(ItemRowsContext::kItemMenu, Controller::kScreen,
                 [&](auto& host, auto&) {
                   return UpdateInventoryScreen(g_screen, *step, host);
                 });
}
void ResetInventoryTerminalModuleData() {
  RequireIdleContext(g_contexts[0]);
  g_contexts[0] = {};
  g_screen = {};
}
void ResetLootInventoryTerminalModuleData() {
  RequireIdleContext(g_contexts[1]);
  g_contexts[1] = {};
}
void ResetMiscItemShortcutModuleData() {
  if (g_contexts[0].active[static_cast<std::size_t>(Controller::kShortcut)]) {
    Refuse("MAINMENU reload during an active shortcut call");
  }
  g_shortcut = {};
}
void ResetInventoryTerminalSession() {
  for (const auto& context : g_contexts) {
    RequireIdleContext(context);
  }
  g_contexts = {};
  g_shortcut = {};
  g_screen = {};
  g_loot_text = {};
}
std::uint64_t InventoryTerminalLiveRefusalCount() noexcept {
  return g_refusals;
}
}  // namespace VS::Menu::Inventory
