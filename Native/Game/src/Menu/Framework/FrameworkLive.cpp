// Hand-maintained (tools/port/owned.csv).
// Live binding for the menu framework (Framework.hpp): defines the
// framework's variables (FrameworkLiveState.h), mirrors the scalar ones with
// the typed State around each call and implements the Host over the workspace
// block, the frame ordering tables, the disc queue, the text boxes, the
// battle interface, the camera and the submenus.
#include "Menu/Framework/FrameworkLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "Actors/ActorStateLive.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Menu/FilesLive.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/OptionsLive.hpp"
#include "Menu/RenameLive.hpp"
#include "Menu/Setup/SetupLive.hpp"
#include "Menu/Sounds.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/EncodedText.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/GimImagesLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Save/NativeContainerLive.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Hud.h"
#include "Battle/Sphere.h"
#include "Battle/TargetingState.h"
#include "Battle/TextBoxes.h"
#include "Boot/OverlayReset.h"
#include "Content/DiscLayout.h"
#include "Core/Overlays.h"
#include "DiscQueue.h"
#include "Effects/ScreenEffects.h"
#include "Field/RoomLoadingLiveState.h"
#include "Input/ControllerLiveState.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Loot.h"
#include "Items/Weapons.h"
#include "Legacy/Psx.h"
#include "Menu/BattleAbilities.h"
#include "Menu/BreakArts.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/MenuMemory.h"
#include "Menu/Framework/MenuRows.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Loot/LootScreen.h"
#include "Menu/Magic.h"
#include "Menu/Manual.h"
#include "Menu/Map.h"
#include "Menu/Results/Results.h"
#include "Menu/Score/ScoreScreen.h"
#include "Menu/Status/ActorStatus.h"
#include "Menu/TextBox.h"
#include "Render/Camera.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/StateFlags.h"
}

namespace VS::Menu::Framework {
namespace {
// D_800EBA78: the main menu help text in words (its module image range).
constexpr std::size_t kHelpTextWords = 80;
}  // namespace
}  // namespace VS::Menu::Framework

static_assert(static_cast<std::size_t>(kMenuSubmenuStateCount) ==
              VS::Menu::Framework::kSubmenuStateCount);

extern "C" {
// decomp: D_800EBA78 (BATTLE/BATTLE.PRG 5BF94.c:2934): the main menu help
// text from the module image (ModuleData.cpp); its last word opens the
// gradient window.
extern unsigned int
    Content_battleMainMenuHelpText[VS::Menu::Framework::kHelpTextWords];

// decomp: _loadedSubMenu (BATTLE/BATTLE.PRG 5BF94.c): the module image byte
// is 0.
signed char Menu_Framework_loadedSubMenu = 0;
// decomp: vs_battle_menuItems (BATTLE/BATTLE.PRG 5BF94.c)
Menu_Framework_MenuRow* Menu_Framework_menuItems = nullptr;
// decomp: D_800EB9C4 (BATTLE/BATTLE.PRG 5BF94.c)
Items_LootListNode* Menu_Framework_lootLists = nullptr;
// decomp: D_800EB9C8 (BATTLE/BATTLE.PRG 5BF94.c)
Items_BattleLoot* Menu_Framework_lootEntry = nullptr;
// decomp: D_800EB9CE (BATTLE/BATTLE.PRG 5BF94.c): the module image byte is 0.
char Menu_textBoxBufferIndex = 0;
// decomp: _menuBgUnpackedBuf (BATTLE/BATTLE.PRG 5BF94.c)
u_int* Menu_Framework_menuBackgroundBuffer = nullptr;
// decomp: D_800F4CC0 (BATTLE/BATTLE.PRG 5BF94.c)
int Menu_Framework_mainMenuCursorAnimation = 0;
// decomp: D_800F4CD0 (BATTLE/BATTLE.PRG 5BF94.c)
u_long Menu_textBoxOrderingTables[kMenuTextBoxOrderingTableCount] = {};
// decomp: vs_battle_shortcutInvoked (BATTLE/BATTLE.PRG 5BF94.c)
char Menu_Framework_shortcutInvoked = 0;
// decomp: D_800F4E6B (BATTLE/BATTLE.PRG 5BF94.c)
char Menu_saveLocationId = 0;
// decomp: vs_battle_submenuStates (BATTLE/BATTLE.PRG 5BF94.c)
char Menu_Framework_submenuStates[kMenuSubmenuStateCount] = {};
// decomp: vs_battle_rowTypeBuf (BATTLE/BATTLE.PRG 5BF94.c)
int* Menu_Framework_rowTypeBuf = nullptr;
// decomp: vs_battle_unlockedBreakArt (BATTLE/BATTLE.PRG 5BF94.c)
char Menu_Framework_unlockedBreakArt = 0;
// decomp: vs_battle_stringBuf (BATTLE/BATTLE.PRG 5BF94.c)
char* Menu_Framework_stringBuf = nullptr;
// decomp: D_800F4E98 (BATTLE/BATTLE.PRG 5BF94.c)
Menu_Framework_ActionSelection Menu_Framework_actionSelection = {};
// decomp: D_800F4EA0 (BATTLE/BATTLE.PRG 5BF94.c)
int Menu_Framework_actionRestrictions = 0;
// decomp: D_800F4EE8 (BATTLE/BATTLE.PRG 5BF94.c)
Menu_Framework_MenuMemory Menu_Framework_menuMemory = {};
// decomp: vs_battle_unlockedBattleAbility (BATTLE/BATTLE.PRG 5BF94.c)
char Menu_Framework_unlockedBattleAbility = 0;
// decomp: D_800F4FDB (BATTLE/BATTLE.PRG 5BF94.c)
char Menu_Framework_openedWithoutMainMenu = 0;
// decomp: vs_battle_menuState (BATTLE/BATTLE.PRG 5BF94.c)
Menu_Framework_MenuControlState Menu_Framework_menuState = {};
// decomp: _itemsText (MENU/MAINMENU.PRG 58EC.c)
u_short* Menu_Framework_itemsText = nullptr;
// decomp: _menuRowYOffset (MENU/MAINMENU.PRG 58EC.c)
char Menu_Framework_menuRowYOffset = 0;
// decomp: _selectedRow (MENU/MAINMENU.PRG 58EC.c)
int Menu_Framework_selectedRow = 0;
// decomp: _subMenuId (MENU/MAINMENU.PRG 58EC.c)
u_char Menu_Framework_subMenuId = 0;
// decomp: _subMenuPage (MENU/MAINMENU.PRG 58EC.c)
char Menu_Framework_subMenuPage = 0;
// decomp: _subMenuRowCount (MENU/MAINMENU.PRG 58EC.c)
u_short Menu_Framework_subMenuRowCount = 0;
// decomp: _subMenuRowInfo (MENU/MAINMENU.PRG 58EC.c)
char Menu_Framework_subMenuRowInfo = 0;
// decomp: _textHeaders (MENU/MAINMENU.PRG 58EC.c)
Menu_Framework_TextHeader
    Menu_Framework_textHeaders[VS::Menu::Framework::kTextHeaderCount] = {};
// decomp: vs_mainMenu_actionMenuState (MENU/MAINMENU.PRG 58EC.c)
char Menu_Framework_actionMenuState = 0;
// decomp: vs_mainMenu_backgroundFadeStep (MENU/MAINMENU.PRG 58EC.c)
int Menu_Framework_backgroundFadeStep = 0;
// decomp: vs_mainMenu_containerEmptyBackup (MENU/MAINMENU.PRG): never read.
char Menu_Framework_unusedContainerEmptyBackup = 0;
// decomp: vs_mainMenu_cursorColor (MENU/MAINMENU.PRG 58EC.c)
char Menu_Framework_cursorColor = 0;
// decomp: vs_mainMenu_displayActionCost (MENU/MAINMENU.PRG 58EC.c)
char Menu_Framework_displayActionCost = 0;
// decomp: vs_mainMenu_hideMenu (MENU/MAINMENU.PRG 58EC.c)
char Menu_Framework_hideMenu = 0;
// decomp: vs_mainMenu_itemsListRow (MENU/MAINMENU.PRG 58EC.c)
char Menu_Framework_itemsListRow = 0;
// decomp: vs_menu_inventoryStorage (MENU/MAINMENU.PRG): never read.
Menu_Container_InventoryStorage* Menu_Framework_unusedInventoryStorage =
    nullptr;
}

namespace VS::Menu::Framework {
namespace {
// The typed records view the live ones in place.
static_assert(std::is_standard_layout_v<MenuRow>);
static_assert(sizeof(MenuRow) == sizeof(Menu_Framework_MenuRow));
static_assert(offsetof(MenuRow, item_page) ==
              offsetof(Menu_Framework_MenuRow, itemPage));
static_assert(offsetof(MenuRow, x) == offsetof(Menu_Framework_MenuRow, x));
static_assert(offsetof(MenuRow, text) ==
              offsetof(Menu_Framework_MenuRow, text));
static_assert(offsetof(MenuRow, sub_text) ==
              offsetof(Menu_Framework_MenuRow, subText));
static_assert(std::is_standard_layout_v<ItemText>);
static_assert(sizeof(ItemText) == sizeof(Menu_Framework_ItemText));
static_assert(offsetof(ItemText, flags) ==
              offsetof(Menu_Framework_ItemText, menuRowFlags));
static_assert(offsetof(ItemText, description) ==
              offsetof(Menu_Framework_ItemText, description));
static_assert(sizeof(TextHeader) == sizeof(Menu_Framework_TextHeader));
static_assert(sizeof(Menu_Framework_MenuMemory) == kMenuMemoryBytes);
static_assert(sizeof(Content_battleMainMenuHelpText) ==
              kHelpTextWords * sizeof(std::uint32_t));

// AcquireLiveOverlayWorkspace's owner tag for the menu table.
constexpr int kMenuWorkspaceOwner = 6;
// vs_battle_stringBuf and vs_battle_rowTypeBuf follow the forty rows.
constexpr std::size_t kRowBytes = sizeof(Menu_Framework_MenuRow) * kRowCount;
constexpr std::size_t kRowTypeBytes = kRowTypeCount * sizeof(int);
constexpr std::size_t kStringsEnd = kRowBytes + kStringBufferBytes;
constexpr std::size_t kWorkspaceBytes = kStringsEnd + kRowTypeBytes;
// The overlay slots: MAINMENU.PRG and the submenus' MENU*.PRG.
constexpr std::size_t kMainMenuOverlaySlot = 1;
constexpr std::size_t kSubmenuOverlaySlot = 2;
// The menu background's encoded image at the submenu slot.
constexpr std::size_t kBackgroundSourceBytes = 0x6000;
// squashLba: the LBA over the size in sectors.
constexpr std::uint32_t kLbaShift = 8;
constexpr std::uint32_t kSectorShift = 11;
constexpr std::uint32_t kSectorCountMask = 0xFF;
// The menu texture pages (ReserveLiveTexturePageRange(0x1A, 3, 6)).
constexpr std::uint32_t kMenuTexturePageFirst = 0x1A;
constexpr std::uint32_t kMenuTexturePageCount = 3;
constexpr int kMenuTexturePageOwner = 6;
// The background CLUT: 256 colours into row 3.
constexpr u_int kBackgroundClutRow = 3;
constexpr u_int kClutColors = 256;
constexpr std::size_t kBackgroundClutBytes = kClutColors * sizeof(u_short);
// Battle_statusIconCarousel with no icon highlighted.
constexpr int kStatusIconCarouselIdle = 0xFFFFFF;
// The text boxes' ordering table tags ClearOTag sets.
constexpr int kTextBoxOrderingTags = 0x22;
// The information box and its portrait.
constexpr int kInformationBox = 7;
constexpr u_int kInformationPortrait = 32;
// Battle_lowerScreenUiState: the raised background bit and the bit kept.
constexpr int kLowerScreenBackgroundRaised = 2;
constexpr int kLowerScreenKept = 1;
// The Ashley preview camera.
constexpr int kPreviewProjectionDistance = 512;
constexpr int kPreviewFarClip = 0x1000;
constexpr std::size_t kAshleyRecord = 1;
// CreateActionRows keeps the row count in a byte.
constexpr std::uint32_t kByteMask = 0xFF;
constexpr int kSubmenuStateMask = 0xF;
// The words of the gradient window copied from the row animation steps.
constexpr std::size_t kGradientRowStepWord = 5;
constexpr Layer kCallerLayer{.base = LayerBase::kCaller, .slot = 0};

[[nodiscard]] constexpr std::uint32_t SquashLba(std::uint32_t lba,
                                                std::uint32_t size) {
  return lba << kLbaShift | size >> kSectorShift;
}

// The item shortcut loads MAINMENU.PRG alone.
[[nodiscard]] constexpr std::uint32_t MainMenuProgram() {
  return SquashLba(VS_MAINMENU_PRG_LBA, VS_MAINMENU_PRG_SIZE);
}

// decomp: _menuLbas (BATTLE/BATTLE.PRG 5BF94.c): MAINMENU.PRG with the
// background, then the submenus' programs.
constexpr std::array<std::uint32_t, kSubmenuStateCount> kMenuFiles = {
    MainMenuProgram() + (VS_MENUBG_BIN_SIZE >> kSectorShift),
    SquashLba(VS_MENU0_PRG_LBA, VS_MENU0_PRG_SIZE),
    SquashLba(VS_MENU1_PRG_LBA, VS_MENU1_PRG_SIZE),
    SquashLba(VS_MENU2_PRG_LBA, VS_MENU2_PRG_SIZE),
    SquashLba(VS_MENU3_PRG_LBA, VS_MENU3_PRG_SIZE),
    SquashLba(VS_MENU4_PRG_LBA, VS_MENU4_PRG_SIZE),
    SquashLba(VS_MENU5_PRG_LBA, VS_MENU5_PRG_SIZE),
    SquashLba(VS_MENU7_PRG_LBA, VS_MENU7_PRG_SIZE),
    SquashLba(VS_MENU8_PRG_LBA, VS_MENU8_PRG_SIZE),
    SquashLba(VS_MENU9_PRG_LBA, VS_MENU9_PRG_SIZE),
    SquashLba(VS_MENUE_PRG_LBA, VS_MENUE_PRG_SIZE),
    SquashLba(VS_MENUA_PRG_LBA, 0),
    SquashLba(VS_MENUB_PRG_LBA, VS_MENUB_PRG_SIZE),
    SquashLba(VS_MENUC_PRG_LBA, VS_MENUC_PRG_SIZE),
    SquashLba(VS_MENUD_PRG_LBA, VS_MENUD_PRG_SIZE),
    SquashLba(VS_MENUF_PRG_LBA, VS_MENUF_PRG_SIZE),
};

State g_state;

// The item lists' blocks and their entry counts.
struct TrackedItems {
  ItemText* items = nullptr;
  std::size_t count = 0;
};
std::array<TrackedItems, 2> g_items{};

[[nodiscard]] TrackedItems& Tracked(ItemList list) {
  return g_items[static_cast<std::size_t>(list)];
}

// The copied code reads plain char as unsigned (/J).
[[nodiscard]] std::uint8_t Byte(char value) {
  return static_cast<std::uint8_t>(value);
}

[[nodiscard]] char Char(std::uint8_t value) { return static_cast<char>(value); }

// The record's signed bytes widen with their sign (lb).
[[nodiscard]] std::int32_t Signed(signed char value) {
  return static_cast<std::int32_t>(value);
}

void LoadControl() {
  g_state.loaded_sub_menu = Menu_Framework_loadedSubMenu;
  g_state.control = {
      .current_menu = Byte(Menu_Framework_menuState.currentMenu),
      .return_state = Byte(Menu_Framework_menuState.returnState),
      .execute_ability_type = Byte(Menu_Framework_menuState.executeAbilityType),
      .loading = Byte(Menu_Framework_menuState.loading)};
  g_state.action_selection = {
      .menu_status = Menu_Framework_actionSelection.menuStatus,
      .selection_kind = Menu_Framework_actionSelection.selectionKind,
      .execute_ability = Menu_Framework_actionSelection.executeAbility.word};
  g_state.shortcut_invoked = Byte(Menu_Framework_shortcutInvoked);
  g_state.opened_without_main_menu = Byte(Menu_Framework_openedWithoutMainMenu);
  g_state.unlocked_break_art = Byte(Menu_Framework_unlockedBreakArt);
  g_state.unlocked_battle_ability = Byte(Menu_Framework_unlockedBattleAbility);
  g_state.save_location_id = Byte(Menu_saveLocationId);
  g_state.action_restrictions = Menu_Framework_actionRestrictions;
  g_state.main_menu_cursor_animation = Menu_Framework_mainMenuCursorAnimation;
  std::memcpy(g_state.submenu_states.data(), Menu_Framework_submenuStates,
              sizeof(Menu_Framework_submenuStates));
  std::memcpy(g_state.text_headers.data(), Menu_Framework_textHeaders,
              sizeof(Menu_Framework_textHeaders));
  g_state.loot_lists =
      LootListId{reinterpret_cast<std::uintptr_t>(Menu_Framework_lootLists)};
  g_state.loot_entry =
      LootEntryId{reinterpret_cast<std::uintptr_t>(Menu_Framework_lootEntry)};
}

void LoadMainMenu() {
  MainMenuStatics& menu = g_state.main_menu;
  menu.menu_row_y_offset = Byte(Menu_Framework_menuRowYOffset);
  menu.selected_row = Menu_Framework_selectedRow;
  menu.sub_menu_id = Menu_Framework_subMenuId;
  menu.sub_menu_page = Byte(Menu_Framework_subMenuPage);
  menu.sub_menu_row_count = Menu_Framework_subMenuRowCount;
  menu.sub_menu_row_info = Byte(Menu_Framework_subMenuRowInfo);
  menu.action_menu_state = Byte(Menu_Framework_actionMenuState);
  menu.background_fade_step = Menu_Framework_backgroundFadeStep;
  menu.cursor_color = Byte(Menu_Framework_cursorColor);
  menu.display_action_cost = Byte(Menu_Framework_displayActionCost);
  menu.hide_menu = Byte(Menu_Framework_hideMenu);
  menu.items_list_row = Byte(Menu_Framework_itemsListRow);

  // A list another writer left in _itemsText holds the rows it counts.
  auto* const items = reinterpret_cast<ItemText*>(Menu_Framework_itemsText);
  TrackedItems& rows = Tracked(ItemList::kRows);
  if (items != rows.items) {
    rows = {.items = items,
            .count = items != nullptr ? Menu_Framework_subMenuRowCount : 0U};
  }
}

void StoreControl() {
  Menu_Framework_loadedSubMenu = g_state.loaded_sub_menu;
  Menu_Framework_menuState.currentMenu = Char(g_state.control.current_menu);
  Menu_Framework_menuState.returnState = Char(g_state.control.return_state);
  Menu_Framework_menuState.executeAbilityType =
      Char(g_state.control.execute_ability_type);
  Menu_Framework_menuState.loading = Char(g_state.control.loading);
  Menu_Framework_actionSelection.menuStatus =
      g_state.action_selection.menu_status;
  Menu_Framework_actionSelection.selectionKind =
      g_state.action_selection.selection_kind;
  Menu_Framework_actionSelection.executeAbility.word =
      g_state.action_selection.execute_ability;
  Menu_Framework_shortcutInvoked = Char(g_state.shortcut_invoked);
  Menu_Framework_openedWithoutMainMenu = Char(g_state.opened_without_main_menu);
  Menu_Framework_unlockedBreakArt = Char(g_state.unlocked_break_art);
  Menu_Framework_unlockedBattleAbility = Char(g_state.unlocked_battle_ability);
  Menu_saveLocationId = Char(g_state.save_location_id);
  Menu_Framework_actionRestrictions = g_state.action_restrictions;
  Menu_Framework_mainMenuCursorAnimation = g_state.main_menu_cursor_animation;
  std::memcpy(Menu_Framework_submenuStates, g_state.submenu_states.data(),
              sizeof(Menu_Framework_submenuStates));
  std::memcpy(Menu_Framework_textHeaders, g_state.text_headers.data(),
              sizeof(Menu_Framework_textHeaders));
  Menu_Framework_lootLists = reinterpret_cast<Items_LootListNode*>(
      static_cast<std::uintptr_t>(g_state.loot_lists));
  Menu_Framework_lootEntry = reinterpret_cast<Items_BattleLoot*>(
      static_cast<std::uintptr_t>(g_state.loot_entry));
}

void StoreMainMenu() {
  const MainMenuStatics& menu = g_state.main_menu;
  Menu_Framework_menuRowYOffset = Char(menu.menu_row_y_offset);
  Menu_Framework_selectedRow = menu.selected_row;
  Menu_Framework_subMenuId = menu.sub_menu_id;
  Menu_Framework_subMenuPage = Char(menu.sub_menu_page);
  Menu_Framework_subMenuRowCount = menu.sub_menu_row_count;
  Menu_Framework_subMenuRowInfo = Char(menu.sub_menu_row_info);
  Menu_Framework_actionMenuState = Char(menu.action_menu_state);
  Menu_Framework_backgroundFadeStep = menu.background_fade_step;
  Menu_Framework_cursorColor = Char(menu.cursor_color);
  Menu_Framework_displayActionCost = Char(menu.display_action_cost);
  Menu_Framework_hideMenu = Char(menu.hide_menu);
  Menu_Framework_itemsListRow = Char(menu.items_list_row);
  Menu_Framework_itemsText =
      reinterpret_cast<u_short*>(Tracked(ItemList::kRows).items);
}

// The live variables into the State.
void Load() {
  LoadControl();
  LoadMainMenu();
}

// The State back into the live variables.
void Store() {
  StoreControl();
  StoreMainMenu();
}

// Around a call into copied code that reads or writes the live variables
// (or re-enters a Live entry): the State is stored before it and loaded
// after it.
class CallOutScope {
 public:
  CallOutScope() { Store(); }
  ~CallOutScope() { Load(); }
  CallOutScope(const CallOutScope&) = delete;
  CallOutScope& operator=(const CallOutScope&) = delete;
  CallOutScope(CallOutScope&&) = delete;
  CallOutScope& operator=(CallOutScope&&) = delete;
};

// _renderAshley's camera backups (MENU _renderAshley.h:27).
struct PreviewBackup {
  int projection_distance = 0;
  MATRIX view_matrix{};
  Render_Camera camera{};
};
PreviewBackup g_preview_backup;

template <typename T>
[[nodiscard]] std::span<const std::uint8_t> BytesOf(const T& value) {
  return {reinterpret_cast<const std::uint8_t*>(&value), sizeof(value)};
}

// The module tables; the gradient window is copied from its sources on each
// call, since the module image loads after the first menu frame can run.
[[nodiscard]] Tables LiveTables() {
  static std::array<std::uint32_t, kGradientWindowWords> gradient{};
  gradient[0] = Content_battleMainMenuHelpText[kHelpTextWords - 1];
  gradient[kGradientWindowTop] = Menu_Framework_gradientTopColors[0];
  gradient[kGradientWindowTop + 1] = Menu_Framework_gradientTopColors[1];
  gradient[kGradientWindowBottom] = Menu_Framework_gradientBottomColors[0];
  gradient[kGradientWindowBottom + 1] = Menu_Framework_gradientBottomColors[1];
  std::memcpy(
      &gradient[kGradientRowStepWord], Menu_Framework_rowAnimationSteps,
      (kGradientWindowWords - kGradientRowStepWord) * sizeof(std::uint32_t));
  return {
      .gradient_colors = gradient,
      .row_animation_steps = BytesOf(Menu_Framework_rowAnimationSteps),
      .ability_group_action_ranges =
          BytesOf(Menu_Framework_abilityGroupActionRanges),
      .cursor_brightness = BytesOf(Menu_Framework_cursorBrightnessAnimation),
      .menu_strings = BytesOf(Menu_menuStrings),
      .glyph_widths = VS::Render::LiveVariableFontGlyphWidths(),
      .main_menu_help_text = BytesOf(Content_battleMainMenuHelpText),
  };
}

// Caller text the original read up to its terminator or a fixed copy length;
// a null pointer is no text.
[[nodiscard]] std::span<const std::uint8_t> Trusted(const char* text) {
  if (text == nullptr) {
    return {};
  }
  return {reinterpret_cast<const std::uint8_t*>(text),
          VS::Render::kTrustedEncodedTextBytes};
}

[[nodiscard]] char* Mutable(std::span<const std::uint8_t> text) {
  return const_cast<char*>(reinterpret_cast<const char*>(text.data()));
}

// The name and description pairs of a row builder's text array.
[[nodiscard]] std::vector<RowText> RowTexts(char* const* text,
                                            std::size_t count) {
  std::vector<RowText> texts(count);
  if (text == nullptr) {
    return texts;
  }
  for (std::size_t i = 0; i < count; ++i) {
    texts[i] = {.name = Trusted(text[i * 2]),
                .description = Trusted(text[(i * 2) + 1])};
  }
  return texts;
}

[[nodiscard]] std::span<const std::int32_t> RowTypeSpan(const int* row_types,
                                                        std::size_t count) {
  if (row_types == nullptr) {
    return {};
  }
  return {row_types, count};
}

template <typename T, std::size_t Rows, std::size_t Columns>
void Flatten(const T (&values)[Rows][Columns],
             std::array<std::int32_t, kSortClassCount>& out) {
  static_assert(Rows * Columns <= kSortClassCount);
  for (std::size_t row = 0; row < Rows; ++row) {
    for (std::size_t column = 0; column < Columns; ++column) {
      out[(row * Columns) + column] = values[row][column];
    }
  }
}

[[nodiscard]] SortStats WeaponStats(const Items_WeaponLoadout& weapon) {
  SortStats stats{.category = Byte(weapon.blade.category),
                  .material = Byte(weapon.blade.material),
                  .range = Byte(weapon.range.range),
                  .damage_type = weapon.damageType,
                  .current_dp = weapon.currentDp,
                  .max_dp = weapon.maxDp,
                  .current_pp = weapon.currentPp,
                  .max_pp = weapon.maxPp,
                  .strength = weapon.currentStr,
                  .intelligence = weapon.currentInt,
                  .agility = weapon.currentAgility};
  Flatten(weapon.classAffinityCurrent.classes, stats.classes);
  Flatten(weapon.classAffinityCurrent.affinities, stats.affinities);
  return stats;
}

[[nodiscard]] SortStats ShieldStats(const Items_ShieldLoadout& shield) {
  SortStats stats{.material = Byte(shield.base.material),
                  .current_dp = shield.currentDp,
                  .max_dp = shield.maxDp,
                  .current_pp = shield.currentPp,
                  .max_pp = shield.maxPp,
                  .strength = shield.currentStr,
                  .intelligence = shield.currentInt,
                  .agility = shield.currentAgility};
  for (std::size_t i = 0; i < kSortTypeCount; ++i) {
    stats.types[i] = Byte(shield.types[i]);
  }
  Flatten(shield.classAffinityCurrent.classes, stats.classes);
  Flatten(shield.classAffinityCurrent.affinities, stats.affinities);
  return stats;
}

// The part's classes run on into its affinities, as in the record.
[[nodiscard]] SortStats EquipmentStats(const Items_EquipmentPartStats& part) {
  SortStats stats{.category = Byte(part.category),
                  .material = Byte(part.material),
                  .range = Byte(part.range.range),
                  .damage_type = Byte(part.damageType),
                  .current_dp = part.currentDp,
                  .max_dp = part.maxDp,
                  .current_pp = part.currentPp,
                  .max_pp = part.maxPp,
                  .strength = part.strength,
                  .intelligence = part.intelligence,
                  .agility = part.agility};
  for (std::size_t i = 0; i < kSortTypeCount; ++i) {
    stats.types[i] = Signed(part.types[i]);
  }
  const std::size_t classes = std::size(part.classes);
  for (std::size_t i = 0; i < classes; ++i) {
    stats.classes[i] = Signed(part.classes[i]);
    stats.classes[classes + i] = Signed(part.affinities[i]);
    stats.affinities[i] = Signed(part.affinities[i]);
  }
  return stats;
}

class LiveHost final : public Host {
 public:
  explicit LiveHost(u_long* caller_chain = nullptr)
      : caller_chain_(caller_chain) {}

  void Refuse(std::string_view what) override {
    VS_LOG_WARN("menu.framework", "{}", what);
  }

  std::span<MenuRow> Rows() override {
    if (Menu_Framework_menuItems == nullptr) {
      return {};
    }
    return {reinterpret_cast<MenuRow*>(Menu_Framework_menuItems), kRowCount};
  }

  std::span<std::uint8_t> Strings() override {
    if (Menu_Framework_menuItems == nullptr ||
        Menu_Framework_stringBuf == nullptr) {
      return {};
    }
    return {reinterpret_cast<std::uint8_t*>(Menu_Framework_stringBuf),
            kStringBufferBytes};
  }

  std::span<std::int32_t> RowTypes() override {
    if (Menu_Framework_menuItems == nullptr ||
        Menu_Framework_rowTypeBuf == nullptr) {
      return {};
    }
    return {Menu_Framework_rowTypeBuf, kRowTypeCount};
  }

  std::span<std::uint8_t> MenuMemory() override {
    return {reinterpret_cast<std::uint8_t*>(&Menu_Framework_menuMemory),
            kMenuMemoryBytes};
  }

  bool AllocateWorkspace() override {
    VS::Content::AcquireLiveOverlayWorkspace(kMenuWorkspaceOwner);
    void* const block =
        VS::Core::Memory::AllocateLiveFromHighEnd(kWorkspaceBytes);
    if (block == nullptr) {
      VS::Content::ReleaseLiveOverlayWorkspace(kMenuWorkspaceOwner);
      Menu_Framework_menuItems = nullptr;
      Menu_Framework_stringBuf = nullptr;
      Menu_Framework_rowTypeBuf = nullptr;
      return false;
    }
    auto* const rows = static_cast<Menu_Framework_MenuRow*>(block);
    Menu_Framework_menuItems = rows;
    Menu_Framework_stringBuf = reinterpret_cast<char*>(rows + kRowCount);
    Menu_Framework_rowTypeBuf =
        reinterpret_cast<int*>(Menu_Framework_stringBuf + kStringBufferBytes);
    return true;
  }

  void ReleaseWorkspace() override {
    if (Menu_Framework_menuItems == nullptr) {
      return;
    }
    VS::Content::ReleaseLiveOverlayWorkspace(kMenuWorkspaceOwner);
    VS::Core::Memory::FreeLiveFromHighEnd(Menu_Framework_menuItems);
    Menu_Framework_menuItems = nullptr;
  }

  void ForgetWorkspace() override { Menu_Framework_menuItems = nullptr; }

  std::span<ItemText> Items(ItemList list) override {
    const TrackedItems& tracked = Tracked(list);
    if (tracked.items == nullptr) {
      return {};
    }
    return {tracked.items, tracked.count};
  }

  bool HasItems(ItemList list) override {
    return Tracked(list).items != nullptr;
  }

  std::span<ItemText> AllocateItems(ItemList list, int count) override {
    const auto entries = static_cast<std::size_t>(std::max(count, 0));
    auto* const items = static_cast<ItemText*>(
        VS::Core::Memory::AllocateLiveFromHighEnd(entries * sizeof(ItemText)));
    TrackedItems& tracked = Tracked(list);
    tracked = {.items = items, .count = items != nullptr ? entries : 0};
    if (list == ItemList::kRows) {
      Menu_Framework_itemsText = reinterpret_cast<u_short*>(items);
    }
    if (items == nullptr) {
      return {};
    }
    return {items, entries};
  }

  void FreeItems(ItemList list) override {
    TrackedItems& tracked = Tracked(list);
    if (tracked.items != nullptr) {
      VS::Core::Memory::FreeLiveFromHighEnd(tracked.items);
    }
    tracked = {};
    if (list == ItemList::kRows) {
      Menu_Framework_itemsText = nullptr;
    }
  }

  Buttons Input() override {
    return {.pressed = Input_buttonsPressed.allButtons,
            .repeat = static_cast<std::uint32_t>(Input_buttonRepeat),
            .held = static_cast<std::uint32_t>(Input_buttonsState)};
  }

  Settings MenuSettings() override {
    return {.cursor_memory = Byte(Input_settings.cursorMemory),
            .information = Byte(Input_settings.information),
            .menu_flags = Input_settings.menuFlags};
  }

  void SetMenuFlags(std::uint32_t flags) override {
    Input_settings.menuFlags = static_cast<std::uint16_t>(flags);
  }

  void PlaySound(MenuSound sound) override {
    switch (sound) {
      case MenuSound::kChange:
        VS::Menu::PlayChangeSound();
        break;
      case MenuSound::kSelect:
        VS::Menu::PlaySelectSound();
        break;
      case MenuSound::kLeave:
        VS::Menu::PlayLeaveSound();
        break;
      case MenuSound::kInvalid:
        VS::Menu::PlayInvalidSound();
        break;
      case MenuSound::kOpen:
        VS::Menu::PlayOpenSound();
        break;
    }
  }

  void InitInformationBox(InformationHeader header) override {
    const CallOutScope scope;
    Menu_InitInformationTextBox(static_cast<Menu_InformationBoxHeader>(header));
  }

  void SetInformationBoxState(std::uint8_t state) override {
    Menu_textBoxes[kInformationBox].state = state;
  }

  void SetInformationText(std::span<const std::uint8_t> text) override {
    if (text.empty()) {
      Refuse("information box text without text");
      return;
    }
    const CallOutScope scope;
    Menu_SetTextBoxText(kInformationBox, Mutable(text));
  }

  void ShowInformationMessage(std::span<const std::uint8_t> text) override {
    if (text.empty()) {
      Refuse("information message without text");
      return;
    }
    Menu_textBoxes[kInformationBox].flags.speakerTexture = kInformationPortrait;
    const CallOutScope scope;
    Menu_SetTextBoxText(kInformationBox, Mutable(text));
  }

  void DismissInformationBox() override {
    const CallOutScope scope;
    Menu_DismissTextBox(kInformationBox);
  }

  void SetStringArgument(std::span<const std::uint8_t> text) override {
    Battle_stringContext.strings[0] = text.empty() ? nullptr : Mutable(text);
  }

  int CurrentSceneId() override { return VS::Field::LiveCurrentSceneId(); }

  std::optional<RestrictionInputs> Restrictions() override {
    const Battle_Actor* const actor = Battle_characterState;
    if (actor == nullptr || actor->stats == nullptr ||
        actor->model == nullptr) {
      Refuse("action restrictions without Ashley");
      return std::nullopt;
    }
    return RestrictionInputs{
        .state_flags = VS::Actors::LiveActorStateFlags(0),
        .weapon_drawn = actor->model->weaponDrawn != 0,
        .mp = actor->stats->currentMp,
        .hp = actor->stats->currentHp,
        .statuses = static_cast<std::uint32_t>(actor->stats->statuses)};
  }

  bool IsActionUnlocked(int action) override {
    if (action < 0 || static_cast<std::size_t>(action) >=
                          std::size(Battle_actionDefinitions)) {
      Refuse("action outside the action table");
      return false;
    }
    return Battle_actionDefinitions[action].unlocked != 0;
  }

  StoryFlags Story() override {
    return {.opaque_b5 = Byte(Audio_Seq_unusedStateFlags.opaqueB5),
            .intro_state = Byte(Audio_Seq_unusedStateFlags.introState),
            .game_over = Byte(Audio_Seq_unusedStateFlags.gameOver)};
  }

  bool HasUnlockedSpellClass(int spell_class) override {
    return VS::Battle::LiveHasUnlockedSpellClass(spell_class) != 0;
  }

  bool HasUnlockedWeaponBreakArt() override {
    return VS::Battle::LiveHasUnlockedWeaponBreakArt() != 0;
  }

  bool HasUnlockedBattleAbility(int ability) override {
    return VS::Battle::LiveHasUnlockedBattleAbility(ability) != 0;
  }

  std::span<const std::uint8_t> AshleyName() override {
    if (Battle_characterState == nullptr ||
        Battle_characterState->stats == nullptr) {
      return {};
    }
    return BytesOf(Battle_characterState->stats->name);
  }

  const char* LimbName(int limb) override {
    if (limb < 0 ||
        static_cast<std::size_t>(limb) >= std::size(Battle_limbNames)) {
      Refuse("limb outside the limb names");
      return nullptr;
    }
    return Battle_limbNames[limb];
  }

  void BeginOverlayLoad(int sub_menu, bool main_menu_program) override {
    Menu_Framework_MenuControlState& control = Menu_Framework_menuState;
    std::uint32_t file_data = MainMenuProgram();
    if (!main_menu_program) {
      if (sub_menu < 0 ||
          static_cast<std::size_t>(sub_menu) >= kMenuFiles.size()) {
        Refuse("menu overlay outside the menu files");
        control.cdQueue = nullptr;
        return;
      }
      file_data = kMenuFiles[static_cast<std::size_t>(sub_menu)];
    }
    Content_DiscFile file{};
    file.lba = static_cast<int>(file_data >> kLbaShift);
    file.size =
        static_cast<int>((file_data & kSectorCountMask) << kSectorShift);
    void* const slot = Core_overlaySlots[sub_menu == 0 ? kMainMenuOverlaySlot
                                                       : kSubmenuOverlaySlot];
    control.cdQueue = VS::Content::AllocateLiveDiscRequest(&file);
    if (control.cdQueue != nullptr) {
      VS::Content::EnqueueLiveDiscRequest(control.cdQueue, slot);
    } else {
      control.cdQueue =
          VS::Content::CompleteLiveRefusedDiscRequest(&file, slot);
    }
  }

  bool IsOverlayLoaded() override {
    const Content_DiscQueueSlot* const request =
        Menu_Framework_menuState.cdQueue;
    return request == nullptr ||
           request->state == Content_DiscRequestState_Loaded;
  }

  void FinishOverlayLoad() override {
    if (Menu_Framework_menuState.cdQueue != nullptr) {
      VS::Content::ReleaseLiveDiscRequest(Menu_Framework_menuState.cdQueue);
    }
    VS::Boot::WaitForOverlayReadyLive();
  }

  void ForgetOverlayLoad() override {
    Menu_Framework_menuState.cdQueue = nullptr;
  }

  // The other MAINMENU.PRG files' statics; the original dropped both lists'
  // pointers without freeing them.
  void ResetMainMenuOverlay() override {
    g_items = {};
    const CallOutScope scope;
    Boot_ResetMainMenuStatics();
    ResetLiveItemRowsStatics();
    ResetLiveEquipmentDetailsStatics();
  }

  bool ReleaseBackgroundBuffer() override {
    if (Menu_Framework_menuBackgroundBuffer == nullptr) {
      return false;
    }
    VS::Core::Memory::FreeLiveFromHighEnd(Menu_Framework_menuBackgroundBuffer);
    Menu_Framework_menuBackgroundBuffer = nullptr;
    return true;
  }

  void StoreEquippedStats() override {
    const CallOutScope scope;
    VS::Items::Inventory::StoreLiveEquippedStats();
  }

  std::span<std::uint32_t> AllocateBackgroundBuffer() override {
    Menu_Framework_menuBackgroundBuffer =
        static_cast<u_int*>(VS::Core::Memory::AllocateLiveFromHighEnd(
            kBackgroundWords * sizeof(u_int)));
    if (Menu_Framework_menuBackgroundBuffer == nullptr) {
      return {};
    }
    return {Menu_Framework_menuBackgroundBuffer, kBackgroundWords};
  }

  void ForgetBackgroundBuffer() override {
    Menu_Framework_menuBackgroundBuffer = nullptr;
  }

  std::span<const std::uint8_t> BackgroundSource() override {
    const auto* const source = static_cast<const std::uint8_t*>(
        Core_overlaySlots[kSubmenuOverlaySlot]);
    if (source == nullptr) {
      return {};
    }
    return {source, kBackgroundSourceBytes};
  }

  void UpdateMiscItemShortcut(int mode) override {
    const CallOutScope scope;
    Menu_Inventory_UpdateMiscItemShortcut(mode);
  }

  int RunSubmenu(Submenu submenu, int state_index) override {
    char& state = Menu_Framework_submenuStates[state_index & kSubmenuStateMask];
    auto* const bytes = reinterpret_cast<u_char*>(&state);
    const CallOutScope scope;
    switch (submenu) {
      case Submenu::kMagic:
        return Menu_Magic_Update(bytes);
      case Submenu::kBreakArts:
        return Menu_BreakArts_Update(bytes);
      case Submenu::kBattleAbilities:
        return Menu_BattleAbilities_Update(bytes);
      case Submenu::kInventory:
        return Menu_Inventory_Update(bytes);
      case Submenu::kStatus:
        return Menu_Status_Update(bytes);
      case Submenu::kMap:
        return Menu_Map_UpdateScreen(bytes);
      case Submenu::kData:
        return VS::Menu::Files::UpdateLiveDataMenu(*bytes);
      case Submenu::kOptions:
        return VS::Menu::Options::UpdateLiveOptions(*bytes);
      case Submenu::kScore:
        return Menu_Score_UpdateScreen(bytes);
      case Submenu::kManual:
        return Menu_Manual_UpdateScreen(bytes);
      case Submenu::kResults:
        return Menu_Results_UpdateScreen(bytes);
      case Submenu::kUnlockNotifications:
        return Menu_BattleAbilities_UpdateUnlockNotifications(&state);
      case Submenu::kContainer:
        return Menu_Container_UpdateScreen(bytes);
      case Submenu::kRename:
        return VS::Menu::Rename::UpdateLiveRename(*bytes);
      case Submenu::kSetup:
        return VS::Menu::Setup::UpdateLiveSetup(*bytes);
      case Submenu::kLoot:
        return Menu_Loot_Update(bytes);
      case Submenu::kContainerSave:
        return VS::Menu::Files::UpdateLiveContainerSave(state);
      case Submenu::kGameOver:
        return VS::Menu::Files::UpdateLiveGameOver(*bytes);
    }
    return 0;
  }

  void BindInventoryViews() override {
    Items_Inventory& inventory = Audio_Seq_unusedInventory;
    Menu_Framework_weapons = inventory.weapons;
    Menu_Framework_blades = inventory.blades;
    Menu_Framework_grips = inventory.grips;
    Menu_Framework_shields = inventory.shields;
    Menu_Framework_armor = inventory.armor;
    Menu_Framework_gems = inventory.gems;
  }

  void UpdateStatPanels() override {
    const CallOutScope scope;
    UpdateLiveEquipmentStatPages(0);
    UpdateLivePrimaryStatPanel(0);
    UpdateLiveDpPpBars(0);
  }

  void ReserveMenuTexturePages() override {
    VS::Render::ReserveLiveTexturePageRange(
        kMenuTexturePageFirst, kMenuTexturePageCount, kMenuTexturePageOwner);
  }

  void RetireMenuTexturePages() override {
    VS::Render::RetireLiveTexturePageRange(
        kMenuTexturePageFirst, kMenuTexturePageCount, kMenuTexturePageOwner);
  }

  void UploadImage(std::uint32_t position, std::span<const std::byte> pixels,
                   std::uint32_t size) override {
    VS::Render::UploadLiveImageRectangle(static_cast<int>(position),
                                         pixels.data(), static_cast<int>(size));
  }

  void ClearImage(const ImageRect& rect) override {
    RECT area{.x = rect.x, .y = rect.y, .w = rect.w, .h = rect.h};
    ::ClearImage(&area, 0, 0, 0);
  }

  void LoadBackgroundClut(std::span<const std::uint8_t> colors) override {
    if (colors.size() < kBackgroundClutBytes) {
      Refuse("menu background CLUT shorter than its colours");
      return;
    }
    VS::Render::LoadLiveClutColors(
        reinterpret_cast<const u_short*>(colors.data()), kBackgroundClutRow, 0,
        kClutColors);
  }

  // func_800CA9C0's resets outside the menu (BATTLE.PRG 5BF94.c:3484).
  void ResetBattleInterface() override {
    const CallOutScope scope;
    Battle_reuseSphereVolumes = 0;
    Battle_hudStatusIconsHidden = 0;
    Battle_hudSlideOffset = 0;
    Effects_screenEffectOverlay = nullptr;
    Battle_targetSphere = nullptr;
    VS::Render::ForgetLiveGimImages();
    Menu_textBoxBufferIndex = 0;
    Battle_lowerScreenUiState = 0;
    Battle_hudHidden = 0;
    Battle_hudBattleMode = 0;
    Battle_battleModeIndicatorStep = 0;
    Battle_statusIconCarousel.word = kStatusIconCarouselIdle;
    Menu_sceneNotificationBuffer = nullptr;
    Effects_ResetScreenEffectState();
    Battle_actionSelection = &Menu_Framework_actionSelection;
    Menu_textBoxes[0] = Menu_TextBoxState{};
    Menu_activeTextBoxOrderingTable = Menu_textBoxOrderingTables;
    ClearOTag(Menu_activeTextBoxOrderingTable, kTextBoxOrderingTags);
  }

  void SetBackgroundRaised(bool raised) override {
    Battle_lowerScreenUiState = static_cast<char>(
        raised ? (Battle_lowerScreenUiState | kLowerScreenBackgroundRaised)
               : (Battle_lowerScreenUiState & kLowerScreenKept));
  }

  Render::PacketQueue& Queue(Layer layer) override {
    return queue_.emplace(Chain(layer));
  }

  Render::FontStyle& FontStyle() override {
    return VS::Render::LiveFontStyle();
  }

  void DrawCompactText(Layer layer, std::string_view text,
                       std::uint32_t position, std::uint32_t color) override {
    u_long* const chain = Chain(layer);
    if (chain == nullptr && layer.base != LayerBase::kCaller) {
      Refuse("compact text outside the frame's ordering table");
      return;
    }
    const std::string terminated(text);
    VS::Render::DrawLiveCompactTextColor(terminated.c_str(),
                                         static_cast<int>(position),
                                         static_cast<int>(color), chain);
  }

  int FrameBufferIndex() override { return Render_frameBuf; }

  bool TabArrowsFrozen() override {
    return Menu_Inventory_freezeTabArrows != 0;
  }

  AshleyPreviewState AshleyPreview() override {
    return {.y_offset = Menu_Inventory_ashleyYOffset,
            .render_state = Menu_Inventory_ashleyRenderState};
  }

  void SetAshleyPreview(const AshleyPreviewState& preview) override {
    Menu_Inventory_ashleyYOffset = preview.y_offset;
    Menu_Inventory_ashleyRenderState = preview.render_state;
  }

  std::optional<AshleyMetrics> Ashley() override {
    const Actors_ActorRecord* const record = Actors_actorRecords[kAshleyRecord];
    if (record == nullptr) {
      Refuse("Ashley preview without Ashley's record");
      return std::nullopt;
    }
    return AshleyMetrics{.camera_distance = record->menuCameraDistance,
                         .collision_height = record->collisionHeight};
  }

  void BeginAshleyPreview() override {
    Render_Camera& camera = Render_frameScratch.camera;
    g_preview_backup.projection_distance = Render_projectionDistance;
    VS::Render::SetLiveCameraProjectionDistance(kPreviewProjectionDistance);
    g_preview_backup.view_matrix = Render_frameScratch.viewMatrix;
    g_preview_backup.camera = camera;
    camera.angles.vx = 0;
    camera.angles.vy = 0;
    camera.angles.vz = 0;
    camera.farClip = kPreviewFarClip;
    VS::Render::BuildLiveCameraViewMatrix();
  }

  void LoadActorPreview() override {
    const CallOutScope scope;
    LoadLiveActorPreview(0);
  }

  void ReleaseActorPreview() override {
    const CallOutScope scope;
    ReleaseLiveActorPreview();
  }

  void EndAshleyPreview() override {
    VS::Render::SetLiveCameraProjectionDistance(
        g_preview_backup.projection_distance);
    Render_frameScratch.viewMatrix = g_preview_backup.view_matrix;
    Render_frameScratch.camera = g_preview_backup.camera;
  }

  void DrawAshleyPreview(const Core::Math::Vec3& look_at,
                         const Core::Math::Vec3& position) override {
    Render_Camera& camera = Render_frameScratch.camera;
    camera.lookAt.vx = look_at.x;
    camera.lookAt.vy = look_at.y;
    camera.lookAt.vz = look_at.z;
    camera.position.vx = position.x;
    camera.position.vy = position.y;
    camera.position.vz = position.z;
    camera.angles.vx = 0;
    camera.angles.vy = 0;
    camera.angles.vz = 0;
    camera.farClip = kPreviewFarClip;
    VS::Render::BuildLiveCameraViewMatrix();
    const CallOutScope scope;
    DrawLiveActorPreview(&Render_frameScratch.viewMatrix);
  }

  void UpdateActorAssetQueue() override {
    VS::Content::UpdateLiveActorAssetQueue();
  }

  int FindFreeActorAssetRequest() override {
    return VS::Content::FindLiveFreeActorAssetRequest();
  }

 private:
  // The ordering table entry a layer links into; null when it lies outside
  // the frame's table.
  [[nodiscard]] u_long* Chain(Layer layer) const {
    switch (layer.base) {
      case LayerBase::kCaller:
        return caller_chain_;
      case LayerBase::kScene:
        return VS::Render::LiveFrameOrderingEntry(
            VS::Render::LiveOrderingBase::kScene, layer.slot);
      case LayerBase::kOverlay:
        return VS::Render::LiveFrameOrderingEntry(
            VS::Render::LiveOrderingBase::kOverlay, layer.slot);
    }
    return nullptr;
  }

  u_long* caller_chain_;
  std::optional<VS::Render::LivePacketQueue> queue_;
};

// One Live entry: the State loaded from the live variables, the typed call,
// then the State stored back.
class LiveCall {
 public:
  explicit LiveCall(u_long* caller_chain = nullptr)
      : host_(caller_chain), tables_(LiveTables()) {
    Load();
  }
  ~LiveCall() { Store(); }
  LiveCall(const LiveCall&) = delete;
  LiveCall& operator=(const LiveCall&) = delete;
  LiveCall(LiveCall&&) = delete;
  LiveCall& operator=(LiveCall&&) = delete;

  [[nodiscard]] Context& Ctx() { return ctx_; }

 private:
  LiveHost host_;
  Tables tables_;
  Context ctx_{.state = g_state, .host = host_, .tables = tables_};
};

[[nodiscard]] Menu_Framework_MenuRow* Legacy(MenuRow& row) {
  return reinterpret_cast<Menu_Framework_MenuRow*>(&row);
}

[[nodiscard]] MenuRow* Typed(Menu_Framework_MenuRow* row) {
  return reinterpret_cast<MenuRow*>(row);
}

void CopyRotation(const Core::Math::Mat3& rotation, MATRIX& matrix) {
  for (std::size_t row = 0; row < rotation.m.size(); ++row) {
    for (std::size_t column = 0; column < rotation.m[row].size(); ++column) {
      matrix.m[row][column] = rotation.m[row][column];
    }
  }
}
}  // namespace

// BATTLE.PRG (5BF94.c).
int LoadLiveMenuOverlay(int packed_menu_state) {
  LiveCall call;
  return LoadMenuOverlay(call.Ctx(), packed_menu_state);
}

Menu_Framework_MenuRow* LiveMenuItem(int row) {
  LiveCall call;
  return Legacy(MenuItem(call.Ctx(), row));
}

Menu_Framework_MenuRow* SetLiveMenuItem(int row, int x, int y, int w,
                                        int background_width, char* text) {
  LiveCall call;
  return Legacy(SetMenuItem(call.Ctx(),
                            {.row = row,
                             .x = x,
                             .y = y,
                             .w = w,
                             .background_width = background_width},
                            Trusted(text)));
}

MenuRow& SetLiveMenuItemText(const RowPlacement& placement,
                             std::span<const std::uint8_t> text) {
  std::array<char, VS::Render::kTrustedEncodedTextBytes> copy{};
  copy.fill(static_cast<char>(VS::Render::kTextTerminator));
  std::memcpy(copy.data(), text.data(), std::min(text.size(), copy.size() - 1));
  LiveCall call;
  return SetMenuItem(call.Ctx(), placement, Trusted(copy.data()));
}

MenuRow& LiveMenuRow(int row) {
  LiveCall call;
  return MenuItem(call.Ctx(), row);
}

int LiveGradientStopColor(u_int gradient, u_int color_index, int brightness) {
  LiveCall call;
  return static_cast<int>(
      GradientStopColor(call.Ctx(), gradient, color_index, brightness));
}

void DrawLiveMenuItem(Menu_Framework_MenuRow* row) {
  if (row == nullptr) {
    VS_LOG_WARN("menu.framework", "drawing an absent menu row");
    return;
  }
  LiveCall call;
  DrawMenuItem(call.Ctx(), *Typed(row));
}

int UpdateLiveMainMenuSelection(int mode) {
  LiveCall call;
  return UpdateMainMenuSelection(call.Ctx(), mode);
}

void UpdateLiveMenuOverlay() {
  LiveCall call;
  UpdateMenuOverlay(call.Ctx());
}

void InitLiveGameInterface(void* interface_texture) {
  std::span<const std::byte> texture;
  if (interface_texture != nullptr) {
    texture = {static_cast<const std::byte*>(interface_texture),
               kInterfaceTextureBytes};
  }
  LiveCall call;
  InitGameInterface(call.Ctx(), texture);
}

void ResumeLiveAfterActionSelection() {
  LiveCall call;
  ResumeAfterActionSelection(call.Ctx());
}

int LiveActionRestrictionFlags(int mode) {
  LiveCall call;
  return ActionRestrictionFlags(call.Ctx(), mode);
}

int OpenLiveGameMenu(int menu_state, int mode) {
  LiveCall call;
  return OpenGameMenu(call.Ctx(), menu_state, mode);
}

int OpenLiveShortcutMenu(int shortcut) {
  LiveCall call;
  return OpenShortcutMenu(call.Ctx(), shortcut);
}

void ReleaseLiveMenuRows() {
  LiveCall call;
  ReleaseMenuRows(call.Ctx());
}

void OpenLiveLootListMenu(Items_LootListNode* lists) {
  LiveCall call;
  OpenLootListMenu(call.Ctx(),
                   LootListId{reinterpret_cast<std::uintptr_t>(lists)});
}

void OpenLiveLootEntryMenu(Items_BattleLoot* entry) {
  LiveCall call;
  OpenLootEntryMenu(call.Ctx(),
                    LootEntryId{reinterpret_cast<std::uintptr_t>(entry)});
}

void OpenLiveChestStorageMenu(int location) {
  LiveCall call;
  OpenChestStorageMenu(call.Ctx(), location);
}

void OpenLiveAbilityUnlockMenu(int art, int ability) {
  LiveCall call;
  OpenAbilityUnlockMenu(call.Ctx(), art, ability);
}

void OpenLiveChallengeMenu() {
  LiveCall call;
  OpenChallengeMenu(call.Ctx());
}

Menu_Framework_ActionSelection* LiveActionSelectionState() {
  {
    LiveCall call;
    static_cast<void>(ActionSelectionState(call.Ctx()));
  }
  return &Menu_Framework_actionSelection;
}

int DrawLiveMainMenuCursor(int step, int position) {
  LiveCall call;
  return DrawMainMenuCursor(call.Ctx(), step, position);
}

// MAINMENU.PRG (58EC.c, C48.c).
void CreateLiveActionRows(int row_count, int sub_menu_id, char* menu_text[],
                          int row_types[]) {
  const std::size_t count = static_cast<std::uint32_t>(row_count) & kByteMask;
  const std::vector<RowText> texts = RowTexts(menu_text, count);
  LiveCall call;
  CreateActionRows(call.Ctx(), row_count, sub_menu_id, texts,
                   RowTypeSpan(row_types, count));
}

int LiveHighlightedActionRow() {
  LiveCall call;
  return HighlightedActionRow(call.Ctx());
}

void CreateLiveActionRowsText(int sub_menu_id, std::span<const RowText> texts,
                              std::span<const std::int32_t> row_types) {
  LiveCall call;
  CreateActionRows(call.Ctx(), static_cast<int>(texts.size()), sub_menu_id,
                   texts, row_types);
}

std::span<std::uint8_t> LiveMenuCursorMemory() {
  LiveHost host;
  return host.MenuMemory();
}

int LiveActionSelectionResult() {
  LiveCall call;
  return ActionSelectionResult(call.Ctx());
}

Menu_Framework_MenuRow* CreateLiveActionRow(int row, int x) {
  LiveCall call;
  return Legacy(CreateActionRow(call.Ctx(), row, x));
}

void UpdateLiveActionMenu() {
  LiveCall call;
  UpdateActionMenu(call.Ctx());
}

void UpdateLiveInformationVisibility(int source_row, int action_menu_state) {
  LiveCall call;
  UpdateInformationVisibility(call.Ctx(), source_row, action_menu_state);
}

void SetLiveCommandLabel(int action) {
  LiveCall call;
  SetCommandLabel(call.Ctx(), action);
}

void DrawLiveCommandLabel() {
  LiveCall call;
  DrawCommandLabel(call.Ctx());
}

void SetLiveBackgroundFade(int fade_state) {
  LiveCall call;
  SetBackgroundFade(call.Ctx(), fade_state);
}

void DeactivateLiveRow(int row) {
  LiveCall call;
  DeactivateRow(call.Ctx(), row);
}

void DismissLiveInformationBox() {
  LiveCall call;
  DismissInformationBox(call.Ctx());
}

void InitializeLiveInformationBox() {
  LiveCall call;
  InitializeInformationBox(call.Ctx());
}

void SetLiveInformationMessage(char* message) {
  LiveCall call;
  SetInformationMessage(call.Ctx(), Trusted(message));
}

void SetLiveInformationMessageText(std::span<const std::uint8_t> message) {
  LiveCall call;
  SetInformationMessage(call.Ctx(), message);
}

void DrawLiveButtonIcon(int button_id, int x, int y, u_long* before) {
  u_long* const chain =
      before != nullptr
          ? before
          : static_cast<u_long*>(Render_frameScratch.overlayOrderingTable);
  LiveCall call(chain);
  DrawButtonIcon(call.Ctx(), button_id, x, y, kCallerLayer);
}

int DrawLiveCursor(u_int step, int position) {
  LiveCall call;
  return DrawCursor(call.Ctx(), step, static_cast<std::uint32_t>(position));
}

int DrawLiveColoredInteger(int value, int xy, int color, u_long* before) {
  LiveCall call(before);
  return static_cast<int>(
      DrawColoredInteger(call.Ctx(),
                         {.value = value,
                          .position = static_cast<std::uint32_t>(xy),
                          .color = static_cast<std::uint32_t>(color)},
                         kCallerLayer));
}

int DrawLiveComparedInteger(int value, int xy, int threshold, u_long* before) {
  LiveCall call(before);
  return static_cast<int>(DrawComparedInteger(call.Ctx(), value,
                                              static_cast<std::uint32_t>(xy),
                                              threshold, kCallerLayer));
}

void DrawLiveNeutralInteger(int value, int xy, u_long* before) {
  LiveCall call(before);
  DrawNeutralInteger(call.Ctx(), value, static_cast<std::uint32_t>(xy),
                     kCallerLayer);
}

void DrawLiveFeatheredRowIcon(int direction, int xy, int uv_clut,
                              u_long* before) {
  LiveCall call(before);
  DrawFeatheredRowIcon(call.Ctx(), direction, static_cast<std::uint32_t>(xy),
                       static_cast<std::uint32_t>(uv_clut), kCallerLayer);
}

void DrawLiveMaterialIndicator(int material, int xy) {
  LiveCall call;
  DrawMaterialIndicator(call.Ctx(), material, static_cast<std::uint32_t>(xy));
}

void DrawLiveEquipmentStateIcon(int icon, int xy) {
  LiveCall call;
  DrawEquipmentStateIcon(call.Ctx(), icon, static_cast<std::uint32_t>(xy));
}

void DrawLiveRowIcon(int icon, int x, int y) {
  LiveCall call;
  DrawRowIcon(call.Ctx(), icon, x, y);
}

void SetLiveActionCostText(int index, char const* text, int x_offset,
                           int disabled) {
  LiveCall call;
  SetActionCostText(call.Ctx(), index, Trusted(text), x_offset, disabled);
}

void DrawLiveActionCostPanel() {
  LiveCall call;
  DrawActionCostPanel(call.Ctx());
}

void DrawLiveButtonPromptBackground(int x, int y, int w, int h) {
  LiveCall call;
  DrawButtonPromptBackground(call.Ctx(), {.x = x, .y = y, .w = w, .h = h});
}

void SetLiveBackgroundPriority(int ot_offset, int brightness) {
  LiveCall call;
  SetBackgroundPriority(call.Ctx(), ot_offset, brightness);
}

void LoadLiveBackgroundTexture(u_int* buffer) {
  if (buffer == nullptr) {
    VS_LOG_WARN("menu.framework", "menu background without a buffer");
    return;
  }
  LiveCall call;
  LoadBackgroundTexture(call.Ctx(), {buffer, kBackgroundWords});
}

void CreateLiveMenuRows(int row_count, int row_info, char* strings[],
                        int row_types[]) {
  const auto count = static_cast<std::size_t>(std::max(row_count, 0));
  const std::vector<RowText> texts = RowTexts(strings, count);
  LiveCall call;
  CreateMenuRows(call.Ctx(), row_count, row_info, texts,
                 RowTypeSpan(row_types, count));
}

int CloseLiveRowsAndRememberCursor() {
  LiveCall call;
  return CloseRowsAndRememberCursor(call.Ctx());
}

int LiveHighlightedRow() {
  LiveCall call;
  return HighlightedRow(call.Ctx());
}

int LiveSelectedRow() {
  LiveCall call;
  return GetSelectedRow(call.Ctx());
}

Menu_Framework_MenuRow* CreateLiveListRow(int row, int x) {
  LiveCall call;
  return Legacy(CreateListRow(call.Ctx(), row, x));
}

void UpdateLiveRowSelection() {
  LiveCall call;
  UpdateRowSelection(call.Ctx());
}

void DrawLiveBackground(int brightness) {
  LiveCall call;
  DrawBackground(call.Ctx(), brightness);
}

int DrawLiveFeatheredRowCharacter(u_int character, int x,
                                  Menu_Framework_MenuRow* row, u_long* before) {
  if (row == nullptr) {
    VS_LOG_WARN("menu.framework", "row character without a menu row");
    return x;
  }
  LiveCall call(before);
  return DrawFeatheredRowCharacter(call.Ctx(), character, x, *Typed(row),
                                   kCallerLayer);
}

void DrawLiveTabNavigation(int mode) {
  LiveCall call;
  DrawTabNavigation(call.Ctx(), mode);
}

void DrawLiveScrollableRow(Menu_Framework_MenuRow* row) {
  if (row == nullptr) {
    VS_LOG_WARN("menu.framework", "drawing an absent scrollable row");
    return;
  }
  LiveCall call;
  DrawScrollableRow(call.Ctx(), *Typed(row));
}

void UpdateAndDrawLiveRows() {
  LiveCall call;
  UpdateAndDrawRows(call.Ctx());
}

void UpdateLiveScreen() {
  LiveCall call;
  UpdateScreen(call.Ctx());
}

int LiveNextFreeActorAssetRequest() {
  LiveCall call;
  return NextFreeActorAssetRequest(call.Ctx());
}

void FlyOutLiveRowLeft(int row) {
  LiveCall call;
  FlyOutRowLeft(call.Ctx(), row);
}

void FlyOutLiveRowsLeftExcept(int row) {
  LiveCall call;
  FlyOutRowsLeftExcept(call.Ctx(), row);
}

void FlyOutLiveRowRight(int row) {
  LiveCall call;
  FlyOutRowRight(call.Ctx(), row);
}

void ClearLiveMenuExcept(int row) {
  LiveCall call;
  ClearMenuExcept(call.Ctx(), row);
}

void FlyOutLiveRowsAndHoistSelection(int row, int offset) {
  LiveCall call;
  FlyOutRowsAndHoistSelection(call.Ctx(), row, offset);
}

int AreLiveMenuRowsIdle() {
  LiveCall call;
  return AreMenuRowsIdle(call.Ctx()) ? 1 : 0;
}

int LiveQuitDelay() {
  LiveCall call;
  return QuitDelay(call.Ctx());
}

int ReturnLiveFromSubmenu(int packed_menu_state) {
  LiveCall call;
  return ReturnFromSubmenu(call.Ctx(), packed_menu_state);
}

void UpdateLiveMainMenu(int packed_menu_state) {
  LiveCall call;
  UpdateMainMenu(call.Ctx(), packed_menu_state);
}

// The MENU*.PRG copies.
void RenderLiveAshley(int mode) {
  LiveCall call;
  RenderAshley(call.Ctx(), mode);
}

void ContinueLiveItemDetailsTransition(int row) {
  LiveCall call;
  ContinueItemDetailsTransition(call.Ctx(), row);
}

void UpdateLiveItemDetailsSelection(int row, char* text[], u_int row_type,
                                    int page) {
  const RowText details =
      text != nullptr
          ? RowText{.name = Trusted(text[0]), .description = Trusted(text[1])}
          : RowText{};
  LiveCall call;
  UpdateItemDetailsSelection(call.Ctx(), row, details, row_type, page);
}

int LiveWeaponSortValue(int stat, Items_WeaponLoadout* weapon) {
  if (weapon == nullptr) {
    VS_LOG_WARN("menu.framework", "sort key of an absent weapon");
    return 0;
  }
  LiveCall call;
  return WeaponSortValue(call.Ctx(), stat, WeaponStats(*weapon));
}

int LiveShieldSortValue(int stat, Items_ShieldLoadout* shield) {
  if (shield == nullptr) {
    VS_LOG_WARN("menu.framework", "sort key of an absent shield");
    return 0;
  }
  LiveCall call;
  return ShieldSortValue(call.Ctx(), stat, ShieldStats(*shield));
}

int LiveMiscSortValue(int stat, Items_InventoryMisc* misc) {
  if (misc == nullptr) {
    VS_LOG_WARN("menu.framework", "sort key of an absent item");
    return 0;
  }
  return MiscSortValue(stat, {.id = misc->id, .count = Byte(misc->count)});
}

int LiveEquipmentSortValue(int stat, Items_EquipmentPartStats* equipment) {
  if (equipment == nullptr) {
    VS_LOG_WARN("menu.framework", "sort key of absent equipment");
    return 0;
  }
  LiveCall call;
  return EquipmentSortValue(call.Ctx(), stat, EquipmentStats(*equipment));
}

void SetLiveMenuVisible(int display) {
  LiveCall call;
  SetMenuVisible(call.Ctx(), display);
}

void ClearLiveMenuExceptTitle() {
  LiveCall call;
  ClearMenuExceptTitle(call.Ctx());
}

void BuildLiveYawMatrix(MATRIX* matrix, std::int16_t angle) {
  if (matrix == nullptr) {
    VS_LOG_WARN("menu.framework", "yaw matrix without a matrix");
    return;
  }
  CopyRotation(BuildYawMatrix(angle), *matrix);
}

void BuildLivePitchMatrix(MATRIX* matrix, std::int16_t angle) {
  if (matrix == nullptr) {
    VS_LOG_WARN("menu.framework", "pitch matrix without a matrix");
    return;
  }
  CopyRotation(BuildPitchMatrix(angle), *matrix);
}

void ShowLiveSecondaryTitle(char* text) {
  LiveCall call;
  ShowSecondaryTitle(call.Ctx(), Trusted(text));
}

void ResetLiveMainMenuStatics() {
  Load();
  g_state.main_menu = {};
  g_items = {};
  Store();
}
}  // namespace VS::Menu::Framework
