// Hand-maintained (tools/port/owned.csv).
// Live binding for the menu framework (Framework.hpp): defines the
// framework's variables (FrameworkLiveState.hpp), mirrors the scalar ones with
// the typed State around each call and implements the Host over the workspace
// block, the frame ordering tables, the disc queue, the text boxes, the
// battle interface, the camera and the submenus.
#include "Menu/Framework/FrameworkLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "Actors/ActorRecord.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/ModelInstance.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Hud.hpp"
#include "Battle/Sphere.hpp"
#include "Battle/TargetingState.hpp"
#include "Battle/TextBoxes.hpp"
#include "Boot/OverlayReset.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscLayout.h"
#include "Content/DiscQueue.hpp"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Overlays.hpp"
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"
#include "Effects/ScreenEffectState.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Field/RoomLoadingLiveState.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/BattleAbilitiesTerminalLive.hpp"
#include "Menu/BattleAbilitiesUnlockLive.hpp"
#include "Menu/BreakArtsLive.hpp"
#include "Menu/Container/ContainerListTransferLive.hpp"
#include "Menu/Container/ItemView.hpp"
#include "Menu/FilesLive.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/EquipmentDetailsLiveState.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Menu/Inventory/Inventory.hpp"
#include "Menu/Inventory/InventoryTerminalLive.hpp"
#include "Menu/Inventory/ItemList.hpp"
#include "Menu/Loot/LootTerminalLive.hpp"
#include "Menu/MagicCostsLive.hpp"
#include "Menu/MagicMenusLive.hpp"
#include "Menu/MagicTerminalLive.hpp"
#include "Menu/ManualScreenLive.hpp"
#include "Menu/MapTerminalLive.hpp"
#include "Menu/OptionsLive.hpp"
#include "Menu/RenameLive.hpp"
#include "Menu/Results/TimeAttackTerminalLive.hpp"
#include "Menu/Score/EncyclopediaPreviewLive.hpp"
#include "Menu/Score/GazetteTerminalLive.hpp"
#include "Menu/Setup/SetupLive.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/Status/TerminalControllersLive.hpp"
#include "Menu/TextBox.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/Camera.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/Display.hpp"
#include "Render/EncodedText.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/Frame.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/GimImagesLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/Render.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"
#include "Save/NativeContainerLive.hpp"
#include "State/StateFlags.hpp"

using VS::Core::Gte::MATRIX;

using VS::Legacy::ClearOTag;
using VS::Legacy::RECT;

namespace VS::Menu::Framework {
namespace {
// D_800EBA78: the main menu help text in words (its module image range).
constexpr std::size_t kHelpTextWords = 80;
}  // namespace
}  // namespace VS::Menu::Framework

// decomp: D_800EBA78 (BATTLE/BATTLE.PRG 5BF94.c:2934): the main menu help
// text from the module image (ModuleData.cpp); its last word opens the
// gradient window.
extern unsigned int
    Content_battleMainMenuHelpText[VS::Menu::Framework::kHelpTextWords];

// decomp: _loadedSubMenu (BATTLE/BATTLE.PRG 5BF94.c): the module image byte
// is 0.
signed char VS::Menu::Framework::loadedSubMenu = 0;
// decomp: vs_battle_menuItems (BATTLE/BATTLE.PRG 5BF94.c)
VS::Menu::Framework::MenuRow* VS::Menu::Framework::menuItems = nullptr;
// decomp: D_800EB9C4 (BATTLE/BATTLE.PRG 5BF94.c)
VS::Items::LootListNode* VS::Menu::Framework::lootLists = nullptr;
// decomp: D_800EB9C8 (BATTLE/BATTLE.PRG 5BF94.c)
VS::Items::BattleLoot* VS::Menu::Framework::lootEntry = nullptr;
// decomp: D_800EB9CE (BATTLE/BATTLE.PRG 5BF94.c): the module image byte is 0.
char VS::Menu::textBoxBufferIndex = 0;
// decomp: _menuBgUnpackedBuf (BATTLE/BATTLE.PRG 5BF94.c)
std::uint32_t* VS::Menu::Framework::menuBackgroundBuffer = nullptr;
// decomp: D_800F4CC0 (BATTLE/BATTLE.PRG 5BF94.c)
int VS::Menu::Framework::mainMenuCursorAnimation = 0;
// decomp: D_800F4CD0 (BATTLE/BATTLE.PRG 5BF94.c)
std::uint32_t
    VS::Menu::textBoxOrderingTables[VS::Menu::kTextBoxOrderingTableCount] = {};
// decomp: vs_battle_shortcutInvoked (BATTLE/BATTLE.PRG 5BF94.c)
char VS::Menu::Framework::shortcutInvoked = 0;
// decomp: D_800F4E6B (BATTLE/BATTLE.PRG 5BF94.c)
char VS::Menu::saveLocationId = 0;
// decomp: vs_battle_submenuStates (BATTLE/BATTLE.PRG 5BF94.c)
char VS::Menu::Framework::submenuStates
    [VS::Menu::Framework::kSubmenuStateCount] = {};
// decomp: vs_battle_rowTypeBuf (BATTLE/BATTLE.PRG 5BF94.c)
int* VS::Menu::Framework::rowTypeBuf = nullptr;
// decomp: vs_battle_unlockedBreakArt (BATTLE/BATTLE.PRG 5BF94.c)
char VS::Menu::Framework::unlockedBreakArt = 0;
// decomp: vs_battle_stringBuf (BATTLE/BATTLE.PRG 5BF94.c)
char* VS::Menu::Framework::stringBuf = nullptr;
// decomp: D_800F4E98 (BATTLE/BATTLE.PRG 5BF94.c)
VS::Menu::Framework::ActionSelection VS::Menu::Framework::actionSelection = {};
// decomp: D_800F4EA0 (BATTLE/BATTLE.PRG 5BF94.c)
int VS::Menu::Framework::actionRestrictions = 0;
// decomp: D_800F4EE8 (BATTLE/BATTLE.PRG 5BF94.c)
VS::Menu::Framework::MenuMemory VS::Menu::Framework::menuMemory = {};
// decomp: vs_battle_unlockedBattleAbility (BATTLE/BATTLE.PRG 5BF94.c)
char VS::Menu::Framework::unlockedBattleAbility = 0;
// decomp: D_800F4FDB (BATTLE/BATTLE.PRG 5BF94.c)
char VS::Menu::Framework::openedWithoutMainMenu = 0;
// decomp: vs_battle_menuState (BATTLE/BATTLE.PRG 5BF94.c)
VS::Menu::Framework::LiveMenuControl VS::Menu::Framework::menuState = {};
// decomp: _itemsText (MENU/MAINMENU.PRG 58EC.c)
std::uint16_t* VS::Menu::Framework::itemsText = nullptr;
// decomp: _menuRowYOffset (MENU/MAINMENU.PRG 58EC.c)
char VS::Menu::Framework::menuRowYOffset = 0;
// decomp: _selectedRow (MENU/MAINMENU.PRG 58EC.c)
int VS::Menu::Framework::selectedRow = 0;
// decomp: _subMenuId (MENU/MAINMENU.PRG 58EC.c)
std::uint8_t VS::Menu::Framework::subMenuId = 0;
// decomp: _subMenuPage (MENU/MAINMENU.PRG 58EC.c)
char VS::Menu::Framework::subMenuPage = 0;
// decomp: _subMenuRowCount (MENU/MAINMENU.PRG 58EC.c)
std::uint16_t VS::Menu::Framework::subMenuRowCount = 0;
// decomp: _subMenuRowInfo (MENU/MAINMENU.PRG 58EC.c)
char VS::Menu::Framework::subMenuRowInfo = 0;
// decomp: _textHeaders (MENU/MAINMENU.PRG 58EC.c)
std::array<VS::Menu::Framework::TextHeader,
           VS::Menu::Framework::kTextHeaderCount>
    VS::Menu::Framework::textHeaders = {};
// decomp: vs_mainMenu_actionMenuState (MENU/MAINMENU.PRG 58EC.c)
char VS::Menu::Framework::actionMenuState = 0;
// decomp: vs_mainMenu_backgroundFadeStep (MENU/MAINMENU.PRG 58EC.c)
int VS::Menu::Framework::backgroundFadeStep = 0;
// decomp: vs_mainMenu_containerEmptyBackup (MENU/MAINMENU.PRG 58EC.c)
char VS::Menu::Framework::containerEmptyBackup = 0;
// decomp: vs_mainMenu_cursorColor (MENU/MAINMENU.PRG 58EC.c)
char VS::Menu::Framework::cursorColor = 0;
// decomp: vs_mainMenu_displayActionCost (MENU/MAINMENU.PRG 58EC.c)
char VS::Menu::Framework::displayActionCost = 0;
// decomp: vs_mainMenu_hideMenu (MENU/MAINMENU.PRG 58EC.c)
char VS::Menu::Framework::hideMenu = 0;
// decomp: vs_mainMenu_itemsListRow (MENU/MAINMENU.PRG 58EC.c)
char VS::Menu::Framework::itemsListRow = 0;
// decomp: vs_menu_inventoryStorage (MENU/MAINMENU.PRG C48.c)
VS::Menu::container::InventoryStorage* VS::Menu::Framework::inventoryStorage =
    nullptr;

namespace VS::Menu::Framework {
namespace {
// The typed records view the live ones in place.
static_assert(std::is_standard_layout_v<MenuRow>);
static_assert(std::is_standard_layout_v<ItemText>);
static_assert(std::is_standard_layout_v<MenuMemory>);
static_assert(sizeof(Content_battleMainMenuHelpText) ==
              kHelpTextWords * sizeof(std::uint32_t));

// AcquireLiveOverlayWorkspace's owner tag for the menu table.
constexpr int kMenuWorkspaceOwner = 6;
// vs_battle_stringBuf and vs_battle_rowTypeBuf follow the forty rows.
constexpr std::size_t kRowBytes = sizeof(MenuRow) * kRowCount;
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
constexpr std::uint32_t kBackgroundClutRow = 3;
constexpr std::uint32_t kClutColors = 256;
constexpr std::size_t kClutColorBytes = sizeof(std::uint16_t);
constexpr std::size_t kBackgroundClutBytes = kClutColors * kClutColorBytes;
// VS::Battle::statusIconCarousel with no icon highlighted.
constexpr int kStatusIconCarouselIdle = 0xFFFFFF;
// The text boxes' ordering table tags ClearOTag sets.
constexpr int kTextBoxOrderingTags = 0x22;
// The information box and its portrait.
constexpr int kInformationBox = 7;
constexpr std::uint32_t kInformationPortrait = 32;
// VS::Battle::lowerScreenUiState: the raised background bit and the bit kept.
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
  g_state.loaded_sub_menu = loadedSubMenu;
  g_state.control = menuState.control;
  g_state.action_selection = actionSelection;
  g_state.shortcut_invoked = Byte(shortcutInvoked);
  g_state.opened_without_main_menu = Byte(openedWithoutMainMenu);
  g_state.unlocked_break_art = Byte(unlockedBreakArt);
  g_state.unlocked_battle_ability = Byte(unlockedBattleAbility);
  g_state.save_location_id = Byte(VS::Menu::saveLocationId);
  g_state.action_restrictions = actionRestrictions;
  g_state.main_menu_cursor_animation = mainMenuCursorAnimation;
  std::memcpy(g_state.submenu_states.data(), submenuStates,
              sizeof(submenuStates));
  g_state.text_headers = textHeaders;
  g_state.loot_lists = LootListId{reinterpret_cast<std::uintptr_t>(lootLists)};
  g_state.loot_entry = LootEntryId{reinterpret_cast<std::uintptr_t>(lootEntry)};
}

void LoadMainMenu() {
  MainMenuStatics& menu = g_state.main_menu;
  menu.menu_row_y_offset = Byte(menuRowYOffset);
  menu.selected_row = selectedRow;
  menu.sub_menu_id = subMenuId;
  menu.sub_menu_page = Byte(subMenuPage);
  menu.sub_menu_row_count = subMenuRowCount;
  menu.sub_menu_row_info = Byte(subMenuRowInfo);
  menu.action_menu_state = Byte(actionMenuState);
  menu.background_fade_step = backgroundFadeStep;
  menu.cursor_color = Byte(cursorColor);
  menu.display_action_cost = Byte(displayActionCost);
  menu.hide_menu = Byte(hideMenu);
  menu.items_list_row = Byte(itemsListRow);

  // A list another writer left in _itemsText holds the rows it counts.
  auto* const items = reinterpret_cast<ItemText*>(itemsText);
  TrackedItems& rows = Tracked(ItemList::kRows);
  if (items != rows.items) {
    rows = {.items = items, .count = items != nullptr ? subMenuRowCount : 0U};
  }
}

void StoreControl() {
  loadedSubMenu = g_state.loaded_sub_menu;
  menuState.control = g_state.control;
  actionSelection = g_state.action_selection;
  shortcutInvoked = Char(g_state.shortcut_invoked);
  openedWithoutMainMenu = Char(g_state.opened_without_main_menu);
  unlockedBreakArt = Char(g_state.unlocked_break_art);
  unlockedBattleAbility = Char(g_state.unlocked_battle_ability);
  VS::Menu::saveLocationId = Char(g_state.save_location_id);
  actionRestrictions = g_state.action_restrictions;
  mainMenuCursorAnimation = g_state.main_menu_cursor_animation;
  std::memcpy(submenuStates, g_state.submenu_states.data(),
              sizeof(submenuStates));
  textHeaders = g_state.text_headers;
  lootLists = reinterpret_cast<VS::Items::LootListNode*>(
      static_cast<std::uintptr_t>(g_state.loot_lists));
  lootEntry = reinterpret_cast<VS::Items::BattleLoot*>(
      static_cast<std::uintptr_t>(g_state.loot_entry));
}

void StoreMainMenu() {
  const MainMenuStatics& menu = g_state.main_menu;
  menuRowYOffset = Char(menu.menu_row_y_offset);
  selectedRow = menu.selected_row;
  subMenuId = menu.sub_menu_id;
  subMenuPage = Char(menu.sub_menu_page);
  subMenuRowCount = menu.sub_menu_row_count;
  subMenuRowInfo = Char(menu.sub_menu_row_info);
  actionMenuState = Char(menu.action_menu_state);
  backgroundFadeStep = menu.background_fade_step;
  cursorColor = Char(menu.cursor_color);
  displayActionCost = Char(menu.display_action_cost);
  hideMenu = Char(menu.hide_menu);
  itemsListRow = Char(menu.items_list_row);
  itemsText = reinterpret_cast<std::uint16_t*>(Tracked(ItemList::kRows).items);
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

// Original vs_battle_loadMenuPrg (5BF94.c:2576) replaces image-backed
// statics with the program bytes. Native owners live outside that image.
void ResetLoadedSubmenu(int lba) {
  switch (lba) {
    case VS_MENU0_PRG_LBA:
      Magic::ResetMagicMenusLiveState();
      Magic::ResetTeleportMenuLiveState();
      break;
    case VS_MENU1_PRG_LBA:
      break_arts::ResetLiveStatics();
      break;
    case VS_MENU2_PRG_LBA:
      VS::Boot::ResetMenu2Statics();
      break;
    case VS_MENU3_PRG_LBA:
      VS::Boot::ResetMenu3Statics();
      break;
    case VS_MENU4_PRG_LBA:
      VS::Boot::ResetMenu4Statics();
      break;
    case VS_MENU5_PRG_LBA:
      VS::Boot::ResetMenu5Statics();
      break;
    case VS_MENU9_PRG_LBA:
      if (!Score::CleanupLiveEnemyPreviewOwnedState()) {
        VS::Core::LogError("menu.framework",
                           "enemy-preview cleanup refused before MENU9 reload");
        std::abort();
      }
      VS::Boot::ResetMenu9Statics();
      break;
    case VS_MENUB_PRG_LBA:
      VS::Boot::ResetMenubStatics();
      break;
    case VS_MENUD_PRG_LBA:
      VS::Boot::ResetMenudStatics();
      break;
    case VS_MENUE_PRG_LBA:
      VS::Boot::ResetMenueStatics();
      break;
    case VS_MENUF_PRG_LBA:
      VS::Boot::ResetMenufStatics();
      break;
    case VS_MENU7_PRG_LBA:
    case VS_MENU8_PRG_LBA:
    case VS_MENUA_PRG_LBA:
    case VS_MENUC_PRG_LBA:
      break;
    default:
      VS::Core::LogWarn("menu.framework",
                        "unknown completed menu program LBA {}", lba);
      break;
  }
}

// _renderAshley's camera backups (MENU _renderAshley.h:27).
struct PreviewBackup {
  int projection_distance = 0;
  MATRIX view_matrix{};
  VS::Render::Camera camera{};
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
  gradient[kGradientWindowTop] = VS::Menu::Framework::gradientTopColors[0];
  gradient[kGradientWindowTop + 1] = VS::Menu::Framework::gradientTopColors[1];
  gradient[kGradientWindowBottom] =
      VS::Menu::Framework::gradientBottomColors[0];
  gradient[kGradientWindowBottom + 1] =
      VS::Menu::Framework::gradientBottomColors[1];
  std::memcpy(
      &gradient[kGradientRowStepWord], VS::Menu::Framework::rowAnimationSteps,
      (kGradientWindowWords - kGradientRowStepWord) * sizeof(std::uint32_t));
  return {
      .gradient_colors = gradient,
      .row_animation_steps = BytesOf(VS::Menu::Framework::rowAnimationSteps),
      .ability_group_action_ranges =
          BytesOf(VS::Menu::Framework::abilityGroupActionRanges),
      .cursor_brightness =
          BytesOf(VS::Menu::Framework::cursorBrightnessAnimation),
      .menu_strings = BytesOf(VS::Menu::menuStrings),
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
void Flatten(const std::array<std::array<T, Columns>, Rows>& values,
             std::array<std::int32_t, kSortClassCount>& out) {
  static_assert(Rows * Columns <= kSortClassCount);
  for (std::size_t row = 0; row < Rows; ++row) {
    for (std::size_t column = 0; column < Columns; ++column) {
      out[(row * Columns) + column] = values[row][column];
    }
  }
}

[[nodiscard]] SortStats WeaponStats(const VS::Items::WeaponLoadout& weapon) {
  SortStats stats{.category = weapon.blade.category,
                  .material = weapon.blade.material,
                  .range = weapon.range.range,
                  .damage_type = weapon.damage_type,
                  .current_dp = weapon.current_dp,
                  .max_dp = weapon.max_dp,
                  .current_pp = weapon.current_pp,
                  .max_pp = weapon.max_pp,
                  .strength = weapon.attributes.current_strength,
                  .intelligence = weapon.attributes.current_intelligence,
                  .agility = weapon.attributes.current_agility};
  Flatten(weapon.class_affinity.classes, stats.classes);
  Flatten(weapon.class_affinity.affinities, stats.affinities);
  return stats;
}

[[nodiscard]] SortStats ShieldStats(const VS::Items::ShieldLoadout& shield) {
  SortStats stats{.material = shield.base.material,
                  .current_dp = shield.current_dp,
                  .max_dp = shield.max_dp,
                  .current_pp = shield.current_pp,
                  .max_pp = shield.max_pp,
                  .strength = shield.attributes.current_strength,
                  .intelligence = shield.attributes.current_intelligence,
                  .agility = shield.attributes.current_agility};
  for (std::size_t i = 0; i < kSortTypeCount; ++i) {
    stats.types[i] = shield.types[i];
  }
  Flatten(shield.class_affinity.classes, stats.classes);
  Flatten(shield.class_affinity.affinities, stats.affinities);
  return stats;
}

// The part's classes run on into its affinities, as in the record.
[[nodiscard]] SortStats EquipmentStats(const VS::Items::EquipmentPart& part) {
  SortStats stats{.category = part.category,
                  .material = part.material,
                  .range = part.range.range,
                  .damage_type = part.damage_type,
                  .current_dp = part.current_dp,
                  .max_dp = part.max_dp,
                  .current_pp = part.current_pp,
                  .max_pp = part.max_pp,
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
  explicit LiveHost(std::uint32_t* caller_chain = nullptr)
      : caller_chain_(caller_chain) {}

  void Refuse(std::string_view what) override {
    VS::Core::LogWarn("menu.framework", "{}", what);
  }

  std::span<MenuRow> Rows() override {
    if (menuItems == nullptr) {
      return {};
    }
    return {menuItems, kRowCount};
  }

  std::span<std::uint8_t> Strings() override {
    if (menuItems == nullptr || stringBuf == nullptr) {
      return {};
    }
    return {reinterpret_cast<std::uint8_t*>(stringBuf), kStringBufferBytes};
  }

  std::span<std::int32_t> RowTypes() override {
    if (menuItems == nullptr || rowTypeBuf == nullptr) {
      return {};
    }
    return {rowTypeBuf, kRowTypeCount};
  }

  std::span<std::uint8_t> MenuMemory() override {
    return {reinterpret_cast<std::uint8_t*>(&menuMemory), kMenuMemoryBytes};
  }

  bool AllocateWorkspace() override {
    VS::Content::AcquireLiveOverlayWorkspace(kMenuWorkspaceOwner);
    void* const block =
        VS::Core::Memory::AllocateLiveFromHighEnd(kWorkspaceBytes);
    if (block == nullptr) {
      VS::Content::ReleaseLiveOverlayWorkspace(kMenuWorkspaceOwner);
      menuItems = nullptr;
      stringBuf = nullptr;
      rowTypeBuf = nullptr;
      return false;
    }
    auto* const rows = static_cast<MenuRow*>(block);
    menuItems = rows;
    stringBuf = reinterpret_cast<char*>(rows + kRowCount);
    rowTypeBuf = reinterpret_cast<int*>(stringBuf + kStringBufferBytes);
    return true;
  }

  void ReleaseWorkspace() override {
    if (menuItems == nullptr) {
      return;
    }
    VS::Content::ReleaseLiveOverlayWorkspace(kMenuWorkspaceOwner);
    VS::Core::Memory::FreeLiveFromHighEnd(menuItems);
    menuItems = nullptr;
  }

  void ForgetWorkspace() override { menuItems = nullptr; }

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
      itemsText = reinterpret_cast<std::uint16_t*>(items);
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
      itemsText = nullptr;
    }
  }

  Buttons Input() override { return LiveButtons(); }

  Settings MenuSettings() override {
    return {.cursor_memory = VS::Input::settings.cursor_memory,
            .information = VS::Input::settings.information,
            .menu_flags = VS::Input::settings.menu_flags};
  }

  void SetMenuFlags(std::uint32_t flags) override {
    VS::Input::settings.menu_flags = static_cast<std::uint16_t>(flags);
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
    VS::Menu::InitLiveInformationTextBox(header);
  }

  void SetInformationBoxState(std::uint8_t state) override {
    textBoxes[kInformationBox].state = state;
  }

  void SetInformationText(std::span<const std::uint8_t> text) override {
    if (text.empty()) {
      Refuse("information box text without text");
      return;
    }
    const CallOutScope scope;
    VS::Menu::SetLiveTextBoxText(kInformationBox, Mutable(text));
  }

  void ShowInformationMessage(std::span<const std::uint8_t> text) override {
    if (text.empty()) {
      Refuse("information message without text");
      return;
    }
    textBoxes[kInformationBox].flags.speaker_texture = kInformationPortrait;
    const CallOutScope scope;
    VS::Menu::SetLiveTextBoxText(kInformationBox, Mutable(text));
  }

  void DismissInformationBox() override {
    const CallOutScope scope;
    VS::Menu::DismissLiveTextBox(kInformationBox);
  }

  void SetStringArgument(std::span<const std::uint8_t> text) override {
    VS::Battle::stringContext.strings[0] =
        text.empty() ? nullptr : Mutable(text);
  }

  int CurrentSceneId() override { return VS::Field::LiveCurrentSceneId(); }

  std::optional<RestrictionInputs> Restrictions() override {
    const VS::Battle::Actor* const actor = VS::Battle::characterState;
    if (actor == nullptr || actor->stats == nullptr ||
        actor->model == nullptr) {
      Refuse("action restrictions without Ashley");
      return std::nullopt;
    }
    return RestrictionInputs{
        .state_flags = VS::Actors::LiveActorStateFlags(0),
        .weapon_drawn = actor->model->weapon_drawn != 0,
        .mp = actor->stats->current_mp,
        .hp = actor->stats->current_hp,
        .statuses = static_cast<std::uint32_t>(actor->stats->statuses)};
  }

  bool IsActionUnlocked(int action) override {
    if (action < 0 || static_cast<std::size_t>(action) >=
                          std::size(VS::Battle::actionDefinitions)) {
      Refuse("action outside the action table");
      return false;
    }
    return VS::Battle::actionDefinitions[action].unlocked != 0;
  }

  StoryFlags Story() override {
    return {.opaque_b5 = VS::State::stateFlags.opaque_b5,
            .intro_state = VS::State::stateFlags.intro_state,
            .game_over = VS::State::stateFlags.game_over};
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
    if (VS::Battle::characterState == nullptr ||
        VS::Battle::characterState->stats == nullptr) {
      return {};
    }
    return BytesOf(VS::Battle::characterState->stats->name);
  }

  const char* LimbName(int limb) override {
    if (limb < 0 ||
        static_cast<std::size_t>(limb) >= std::size(VS::Battle::limbNames)) {
      Refuse("limb outside the limb names");
      return nullptr;
    }
    return VS::Battle::limbNames[limb];
  }

  void BeginOverlayLoad(int sub_menu, bool main_menu_program) override {
    VS::Content::DiscQueueSlot*& request = menuState.cd_queue;
    std::uint32_t file_data = MainMenuProgram();
    if (!main_menu_program) {
      if (sub_menu < 0 ||
          static_cast<std::size_t>(sub_menu) >= kMenuFiles.size()) {
        Refuse("menu overlay outside the menu files");
        request = nullptr;
        return;
      }
      file_data = kMenuFiles[static_cast<std::size_t>(sub_menu)];
    }
    VS::Content::DiscFile file{};
    file.lba = static_cast<int>(file_data >> kLbaShift);
    file.size =
        static_cast<int>((file_data & kSectorCountMask) << kSectorShift);
    void* const slot =
        VS::Core::kOverlaySlots[sub_menu == 0 ? kMainMenuOverlaySlot
                                              : kSubmenuOverlaySlot];
    request = VS::Content::AllocateLiveDiscRequest(&file);
    if (request != nullptr) {
      VS::Content::EnqueueLiveDiscRequest(request, slot);
    } else {
      request = VS::Content::CompleteLiveRefusedDiscRequest(&file, slot);
    }
  }

  bool IsOverlayLoaded() override {
    const VS::Content::DiscQueueSlot* const request = menuState.cd_queue;
    return request == nullptr ||
           request->state == VS::Content::DiscRequestState::kLoaded;
  }

  void FinishOverlayLoad() override {
    std::optional<int> loaded_lba;
    if (auto* request = menuState.cd_queue) {
      loaded_lba = request->file.lba;
      VS::Content::ReleaseLiveDiscRequest(request);
    }
    VS::Boot::WaitForOverlayReadyLive();
    if (!loaded_lba) {
      return;
    }
    if (*loaded_lba == VS_MAINMENU_PRG_LBA) {
      // The core handles ordinary MAINMENU loads; the item shortcut can
      // load that same program for a nonzero logical submenu.
      if (g_state.loaded_sub_menu != 0) {
        g_state.main_menu = {};
        ResetMainMenuOverlay();
      }
      return;
    }
    const CallOutScope scope;
    ResetLoadedSubmenu(*loaded_lba);
  }

  void ForgetOverlayLoad() override { menuState.cd_queue = nullptr; }

  // The other MAINMENU.PRG files' statics; the original dropped both lists'
  // pointers without freeing them.
  void ResetMainMenuOverlay() override {
    g_items = {};
    const CallOutScope scope;
    VS::Boot::ResetMainMenuStatics();
    ResetLiveItemRowsStatics();
    ResetLiveEquipmentDetailsStatics();
  }

  bool ReleaseBackgroundBuffer() override {
    if (menuBackgroundBuffer == nullptr) {
      return false;
    }
    VS::Core::Memory::FreeLiveFromHighEnd(menuBackgroundBuffer);
    menuBackgroundBuffer = nullptr;
    return true;
  }

  void StoreEquippedStats() override {
    const CallOutScope scope;
    VS::Items::Inventory::StoreLiveEquippedStats();
  }

  std::span<std::uint32_t> AllocateBackgroundBuffer() override {
    menuBackgroundBuffer =
        static_cast<std::uint32_t*>(VS::Core::Memory::AllocateLiveFromHighEnd(
            kBackgroundWords * sizeof(std::uint32_t)));
    if (menuBackgroundBuffer == nullptr) {
      return {};
    }
    return {menuBackgroundBuffer, kBackgroundWords};
  }

  void ForgetBackgroundBuffer() override { menuBackgroundBuffer = nullptr; }

  std::span<const std::uint8_t> BackgroundSource() override {
    const auto* const source = static_cast<const std::uint8_t*>(
        VS::Core::kOverlaySlots[kSubmenuOverlaySlot]);
    if (source == nullptr) {
      return {};
    }
    return {source, kBackgroundSourceBytes};
  }

  void UpdateMiscItemShortcut(int mode) override {
    const CallOutScope scope;
    Inventory::UpdateLiveMiscItemShortcut(mode);
  }

  int RunSubmenu(Submenu submenu, int state_index) override {
    char& state = submenuStates[state_index & kSubmenuStateMask];
    auto* const bytes = reinterpret_cast<std::uint8_t*>(&state);
    const CallOutScope scope;
    switch (submenu) {
      case Submenu::kMagic:
        return Magic::UpdateLive(bytes);
      case Submenu::kBreakArts:
        return VS::Menu::break_arts::UpdateLive(bytes);
      case Submenu::kBattleAbilities:
        return BattleAbilities::LiveUpdateAbilityScreen(bytes);
      case Submenu::kInventory:
        return Inventory::UpdateLiveInventoryScreen(bytes);
      case Submenu::kStatus:
        return Status::LiveUpdateStatusTerminal(bytes);
      case Submenu::kMap:
        return map::LiveUpdateScreen(bytes);
      case Submenu::kData:
        return VS::Menu::Files::UpdateLiveDataMenu(*bytes);
      case Submenu::kOptions:
        return VS::Menu::Options::UpdateLiveOptions(*bytes);
      case Submenu::kScore:
        return VS::Menu::Score::UpdateLiveGazetteScreen(bytes);
      case Submenu::kManual:
        return VS::Menu::manual::LiveUpdateScreen(bytes);
      case Submenu::kResults:
        return Results::LiveUpdateResultsScreen(bytes);
      case Submenu::kUnlockNotifications:
        return BattleAbilities::LiveUpdateUnlockNotifications(&state);
      case Submenu::kContainer:
        return VS::Menu::container::LiveUpdateScreen(bytes);
      case Submenu::kRename:
        return VS::Menu::Rename::UpdateLiveRename(*bytes);
      case Submenu::kSetup:
        return VS::Menu::Setup::UpdateLiveSetup(*bytes);
      case Submenu::kLoot:
        return Loot::UpdateLiveLootScreen(bytes);
      case Submenu::kContainerSave:
        return VS::Menu::Files::UpdateLiveContainerSave(state);
      case Submenu::kGameOver:
        return VS::Menu::Files::UpdateLiveGameOver(*bytes);
    }
    return 0;
  }

  void BindInventoryViews() override {
    VS::Items::InventoryContents& inventory = VS::Items::inventory;
    VS::Menu::Framework::weapons = inventory.weapons.data();
    VS::Menu::Framework::blades = inventory.blades.data();
    VS::Menu::Framework::grips = inventory.grips.data();
    VS::Menu::Framework::shields = inventory.shields.data();
    VS::Menu::Framework::armor = inventory.armor.data();
    VS::Menu::Framework::gems = inventory.gems.data();
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
    VS::Legacy::ClearImage(&area, 0, 0, 0);
  }

  void LoadBackgroundClut(std::span<const std::uint8_t> colors) override {
    if (colors.size() < kBackgroundClutBytes) {
      Refuse("menu background CLUT shorter than its colours");
      return;
    }
    VS::Render::LoadLiveClutColors(
        reinterpret_cast<const std::uint16_t*>(colors.data()),
        kBackgroundClutRow, 0, kClutColors);
  }

  // func_800CA9C0's resets outside the menu (BATTLE.PRG 5BF94.c:3484).
  void ResetBattleInterface() override {
    const CallOutScope scope;
    VS::Battle::reuseSphereVolumes = 0;
    VS::Battle::hudStatusIconsHidden = 0;
    VS::Battle::hudSlideOffset = 0;
    VS::Effects::screenEffectOverlay = nullptr;
    VS::Battle::targetSphere = nullptr;
    VS::Render::ForgetLiveGimImages();
    VS::Menu::textBoxBufferIndex = 0;
    VS::Battle::lowerScreenUiState = 0;
    VS::Battle::hudHidden = 0;
    VS::Battle::hudBattleMode = 0;
    VS::Battle::battleModeIndicatorStep = 0;
    VS::Battle::statusIconCarousel.word = kStatusIconCarouselIdle;
    sceneNotificationBuffer = nullptr;
    VS::Effects::ScreenDistortion::ResetLiveScreenOverlay();
    VS::Battle::actionSelection = &actionSelection;
    textBoxes[0] = TextBoxState{};
    activeTextBoxOrderingTable = VS::Menu::textBoxOrderingTables;
    ClearOTag(activeTextBoxOrderingTable, kTextBoxOrderingTags);
  }

  void SetBackgroundRaised(bool raised) override {
    VS::Battle::lowerScreenUiState = static_cast<char>(
        raised ? (VS::Battle::lowerScreenUiState | kLowerScreenBackgroundRaised)
               : (VS::Battle::lowerScreenUiState & kLowerScreenKept));
  }

  Render::PacketQueue& Queue(Layer layer) override {
    return queue_.emplace(Chain(layer));
  }

  Render::FontStyle& FontStyle() override {
    return VS::Render::LiveFontStyle();
  }

  void DrawCompactText(Layer layer, std::string_view text,
                       std::uint32_t position, std::uint32_t color) override {
    std::uint32_t* const chain = Chain(layer);
    if (chain == nullptr && layer.base != LayerBase::kCaller) {
      Refuse("compact text outside the frame's ordering table");
      return;
    }
    const std::string terminated(text);
    VS::Render::DrawLiveCompactTextColor(terminated.c_str(),
                                         static_cast<int>(position),
                                         static_cast<int>(color), chain);
  }

  int FrameBufferIndex() override { return VS::Render::frameBuf; }

  bool TabArrowsFrozen() override {
    return VS::Menu::Inventory::freezeTabArrows != 0;
  }

  AshleyPreviewState AshleyPreview() override {
    return {.y_offset = VS::Menu::Inventory::ashleyYOffset,
            .render_state = VS::Menu::Inventory::ashleyRenderState};
  }

  void SetAshleyPreview(const AshleyPreviewState& preview) override {
    VS::Menu::Inventory::ashleyYOffset = preview.y_offset;
    VS::Menu::Inventory::ashleyRenderState = preview.render_state;
  }

  std::optional<AshleyMetrics> Ashley() override {
    const VS::Actors::ActorRecord* const record =
        VS::Actors::actorRecords[kAshleyRecord];
    if (record == nullptr) {
      Refuse("Ashley preview without Ashley's record");
      return std::nullopt;
    }
    return AshleyMetrics{.camera_distance = record->menu_camera_distance,
                         .collision_height = record->collision_height};
  }

  void BeginAshleyPreview() override {
    VS::Render::Camera& camera = VS::Render::FrameScratchpad().camera;
    g_preview_backup.projection_distance = VS::Render::projectionDistance;
    VS::Render::SetLiveCameraProjectionDistance(kPreviewProjectionDistance);
    g_preview_backup.view_matrix = VS::Render::FrameScratchpad().view_matrix;
    g_preview_backup.camera = camera;
    camera.angles.vx = 0;
    camera.angles.vy = 0;
    camera.angles.vz = 0;
    camera.far_clip = kPreviewFarClip;
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
    VS::Render::FrameScratchpad().view_matrix = g_preview_backup.view_matrix;
    VS::Render::FrameScratchpad().camera = g_preview_backup.camera;
  }

  void DrawAshleyPreview(const Core::Math::Vec3& look_at,
                         const Core::Math::Vec3& position) override {
    VS::Render::Camera& camera = VS::Render::FrameScratchpad().camera;
    camera.look_at.vx = look_at.x;
    camera.look_at.vy = look_at.y;
    camera.look_at.vz = look_at.z;
    camera.position.vx = position.x;
    camera.position.vy = position.y;
    camera.position.vz = position.z;
    camera.angles.vx = 0;
    camera.angles.vy = 0;
    camera.angles.vz = 0;
    camera.far_clip = kPreviewFarClip;
    VS::Render::BuildLiveCameraViewMatrix();
    const CallOutScope scope;
    DrawLiveActorPreview(&VS::Render::FrameScratchpad().view_matrix);
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
  [[nodiscard]] std::uint32_t* Chain(Layer layer) const {
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

  std::uint32_t* caller_chain_;
  std::optional<VS::Render::LivePacketQueue> queue_;
};

// One Live entry: the State loaded from the live variables, the typed call,
// then the State stored back.
class LiveCall {
 public:
  explicit LiveCall(std::uint32_t* caller_chain = nullptr)
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

MenuRow* LiveMenuItem(int row) {
  LiveCall call;
  return &MenuItem(call.Ctx(), row);
}

MenuRow* SetLiveMenuItem(int row, int x, int y, int w, int background_width,
                         char* text) {
  LiveCall call;
  return &SetMenuItem(call.Ctx(),
                      {.row = row,
                       .x = x,
                       .y = y,
                       .w = w,
                       .background_width = background_width},
                      Trusted(text));
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

Buttons LiveButtons() {
  return {.pressed = VS::Input::buttonsPressed,
          .repeat = static_cast<std::uint32_t>(VS::Input::buttonRepeat),
          .held = static_cast<std::uint32_t>(VS::Input::buttonsState)};
}

int LiveGradientStopColor(std::uint32_t gradient, std::uint32_t color_index,
                          int brightness) {
  LiveCall call;
  return static_cast<int>(
      GradientStopColor(call.Ctx(), gradient, color_index, brightness));
}

void DrawLiveMenuItem(MenuRow* row) {
  if (row == nullptr) {
    VS::Core::LogWarn("menu.framework", "drawing an absent menu row");
    return;
  }
  LiveCall call;
  DrawMenuItem(call.Ctx(), *row);
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

void OpenLiveLootListMenu(VS::Items::LootListNode* lists) {
  LiveCall call;
  OpenLootListMenu(call.Ctx(),
                   LootListId{reinterpret_cast<std::uintptr_t>(lists)});
}

void OpenLiveLootEntryMenu(VS::Items::BattleLoot* entry) {
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

ActionSelection* LiveActionSelectionState() {
  {
    LiveCall call;
    static_cast<void>(ActionSelectionState(call.Ctx()));
  }
  return &actionSelection;
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

MenuRow* CreateLiveActionRow(int row, int x) {
  LiveCall call;
  return &CreateActionRow(call.Ctx(), row, x);
}

void UpdateLiveActionMenu() {
  LiveCall call;
  UpdateActionMenu(call.Ctx());
}

void UpdateLiveInformationVisibility(int source_row, int action_menu_state) {
  LiveCall call;
  UpdateInformationVisibility(call.Ctx(), source_row, action_menu_state);
}

void SetLiveCommandLabel(MenuCommand action) {
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

void DrawLiveButtonIcon(ButtonIcon button, int x, int y,
                        std::uint32_t* before) {
  std::uint32_t* const chain =
      before != nullptr
          ? before
          : static_cast<std::uint32_t*>(
                VS::Render::FrameScratchpad().overlay_ordering_table);
  LiveCall call(chain);
  DrawButtonIcon(call.Ctx(), button, x, y, kCallerLayer);
}

int DrawLiveCursor(std::uint32_t step, int position) {
  LiveCall call;
  return DrawCursor(call.Ctx(), step, static_cast<std::uint32_t>(position));
}

int DrawLiveColoredInteger(int value, int xy, int color,
                           std::uint32_t* before) {
  LiveCall call(before);
  return static_cast<int>(
      DrawColoredInteger(call.Ctx(),
                         {.value = value,
                          .position = static_cast<std::uint32_t>(xy),
                          .color = static_cast<std::uint32_t>(color)},
                         kCallerLayer));
}

int DrawLiveComparedInteger(int value, int xy, int threshold,
                            std::uint32_t* before) {
  LiveCall call(before);
  return static_cast<int>(DrawComparedInteger(call.Ctx(), value,
                                              static_cast<std::uint32_t>(xy),
                                              threshold, kCallerLayer));
}

void DrawLiveNeutralInteger(int value, int xy, std::uint32_t* before) {
  LiveCall call(before);
  DrawNeutralInteger(call.Ctx(), value, static_cast<std::uint32_t>(xy),
                     kCallerLayer);
}

void DrawLiveFeatheredRowIcon(int direction, int xy, int uv_clut,
                              std::uint32_t* before) {
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

void LoadLiveBackgroundTexture(std::uint32_t* buffer) {
  if (buffer == nullptr) {
    VS::Core::LogWarn("menu.framework", "menu background without a buffer");
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

MenuRow* CreateLiveListRow(int row, int x) {
  LiveCall call;
  return &CreateListRow(call.Ctx(), row, x);
}

void UpdateLiveRowSelection() {
  LiveCall call;
  UpdateRowSelection(call.Ctx());
}

void DrawLiveBackground(int brightness) {
  LiveCall call;
  DrawBackground(call.Ctx(), brightness);
}

int DrawLiveFeatheredRowCharacter(std::uint32_t character, int x, MenuRow* row,
                                  std::uint32_t* before) {
  if (row == nullptr) {
    VS::Core::LogWarn("menu.framework", "row character without a menu row");
    return x;
  }
  LiveCall call(before);
  return DrawFeatheredRowCharacter(call.Ctx(), character, x, *row,
                                   kCallerLayer);
}

void DrawLiveTabNavigation(int mode) {
  LiveCall call;
  DrawTabNavigation(call.Ctx(), mode);
}

void DrawLiveScrollableRow(MenuRow* row) {
  if (row == nullptr) {
    VS::Core::LogWarn("menu.framework", "drawing an absent scrollable row");
    return;
  }
  LiveCall call;
  DrawScrollableRow(call.Ctx(), *row);
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
void RenderLiveAshley(AshleyRenderMode mode) {
  LiveCall call;
  RenderAshley(call.Ctx(), mode);
}

void ContinueLiveItemDetailsTransition(int row) {
  LiveCall call;
  ContinueItemDetailsTransition(call.Ctx(), row);
}

void UpdateLiveItemDetailsSelection(int row, char* text[],
                                    std::uint32_t row_type, int page) {
  const RowText details =
      text != nullptr
          ? RowText{.name = Trusted(text[0]), .description = Trusted(text[1])}
          : RowText{};
  LiveCall call;
  UpdateItemDetailsSelection(call.Ctx(), row, details, row_type, page);
}

int LiveWeaponSortValue(int stat, const VS::Items::WeaponLoadout* weapon) {
  if (weapon == nullptr) {
    VS::Core::LogWarn("menu.framework", "sort key of an absent weapon");
    return 0;
  }
  LiveCall call;
  return WeaponSortValue(call.Ctx(), stat, WeaponStats(*weapon));
}

int LiveShieldSortValue(int stat, const VS::Items::ShieldLoadout* shield) {
  if (shield == nullptr) {
    VS::Core::LogWarn("menu.framework", "sort key of an absent shield");
    return 0;
  }
  LiveCall call;
  return ShieldSortValue(call.Ctx(), stat, ShieldStats(*shield));
}

int LiveMiscSortValue(int stat, const VS::Items::InventoryMisc* misc) {
  if (misc == nullptr) {
    VS::Core::LogWarn("menu.framework", "sort key of an absent item");
    return 0;
  }
  return MiscSortValue(stat, {.id = misc->id, .count = misc->count});
}

int LiveEquipmentSortValue(int stat,
                           const VS::Items::EquipmentPart* equipment) {
  if (equipment == nullptr) {
    VS::Core::LogWarn("menu.framework", "sort key of absent equipment");
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
    VS::Core::LogWarn("menu.framework", "yaw matrix without a matrix");
    return;
  }
  CopyRotation(BuildYawMatrix(angle), *matrix);
}

void BuildLivePitchMatrix(MATRIX* matrix, std::int16_t angle) {
  if (matrix == nullptr) {
    VS::Core::LogWarn("menu.framework", "pitch matrix without a matrix");
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
