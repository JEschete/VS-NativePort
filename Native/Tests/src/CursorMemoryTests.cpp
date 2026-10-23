#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Abilities.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Targeting.h"
#include "Core/Memory.h"
#include "Core/Memory/Heap.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Menu/BreakArts.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/MenuRows.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Options.h"
#include "Menu/Setup/Assembly.h"
#include "Menu/Setup/Combination.h"
#include "Menu/Setup/SetupScreen.h"
#include "State/StateFlags.h"
}

namespace {
struct CursorFixture {
  State_Settings saved_settings = Input_settings;
  Core_HeapBlockHeader saved_heap = Core_legacyHeapList;
  Core_HeapBlockHeader saved_secondary = Core_legacySecondaryHeapList;
  Menu_Framework_MenuMemory saved_memory = Menu_Framework_menuMemory;
  Menu_Framework_MenuRow* saved_items = Menu_Framework_menuItems;
  u_short* saved_text = Menu_Framework_itemsText;
  int saved_selected = Menu_Framework_selectedRow;
  int saved_animation = Menu_Framework_mainMenuCursorAnimation;
  char saved_root = Menu_Framework_submenuStates[0];
  char saved_page = Menu_Framework_subMenuPage;
  char saved_row_info = Menu_Framework_subMenuRowInfo;
  char saved_row_y = Menu_Framework_menuRowYOffset;
  char saved_list_row = Menu_Framework_itemsListRow;
  u_char saved_id = Menu_Framework_subMenuId;
  u_short saved_count = Menu_Framework_subMenuRowCount;
  std::array<Core_HeapBlockHeader, 512> heap{};
  struct {
    std::array<Menu_Framework_MenuRow, 40> rows{};
    std::array<std::byte, 0x124> scratch{};
  } workspace;
  std::array<char*, 40> strings{};
  std::array<int, 20> row_types{};

  CursorFixture() {
    Core_Memory_InitializeHeap(heap.data(), sizeof(heap));
    Menu_Framework_menuItems = workspace.rows.data();
    Menu_Framework_itemsText = nullptr;
    Menu_Framework_menuMemory = {};
    Input_settings.menuFlags = 0x3FF;
  }

  ~CursorFixture() {
    if (Menu_Framework_itemsText != nullptr) {
      Core_Memory_FreeFromHighEnd(Menu_Framework_itemsText);
    }
    Input_settings = saved_settings;
    Core_legacyHeapList = saved_heap;
    Core_legacySecondaryHeapList = saved_secondary;
    Menu_Framework_menuMemory = saved_memory;
    Menu_Framework_menuItems = saved_items;
    Menu_Framework_itemsText = saved_text;
    Menu_Framework_selectedRow = saved_selected;
    Menu_Framework_mainMenuCursorAnimation = saved_animation;
    Menu_Framework_submenuStates[0] = saved_root;
    Menu_Framework_subMenuPage = saved_page;
    Menu_Framework_subMenuRowInfo = saved_row_info;
    Menu_Framework_menuRowYOffset = saved_row_y;
    Menu_Framework_itemsListRow = saved_list_row;
    Menu_Framework_subMenuId = saved_id;
    Menu_Framework_subMenuRowCount = saved_count;
  }

  void OpenRows(int count) {
    Menu_Framework_CreateMenuRows(count, 0x100, strings.data(),
                                  row_types.data());
  }
};

struct PuzzleOptionsFixture {
  CursorFixture rows;
  std::array<u_short, 1070> saved_table =
      std::to_array(Menu_Options_menuStrings);
  char saved_disabled = Audio_Seq_unusedStateFlags.puzzleModeDisabled;

  PuzzleOptionsFixture() {
    std::fill_n(Menu_Options_menuStrings, saved_table.size(), 0xE7E7);
    std::fill_n(Menu_Options_menuStrings, 18, 64);
  }

  ~PuzzleOptionsFixture() {
    std::copy(saved_table.begin(), saved_table.end(), Menu_Options_menuStrings);
    Audio_Seq_unusedStateFlags.puzzleModeDisabled = saved_disabled;
  }
};

struct BreakArtMenuFixture {
  CursorFixture rows;
  std::array<u_short, 2151> saved_strings =
      std::to_array(Menu_BreakArts_strings);
  std::array<Battle_ActionDefinition, 4> saved_actions{};
  Battle_Actor* saved_character = Battle_characterState;
  Battle_Actor* saved_actor = Battle_actors[0];
  decltype(Menu_Framework_shortcutInvoked) saved_shortcut =
      Menu_Framework_shortcutInvoked;
  decltype(Menu_Framework_actionRestrictions) saved_restrictions =
      Menu_Framework_actionRestrictions;
  decltype(Menu_Framework_displayActionCost) saved_cost =
      Menu_Framework_displayActionCost;
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  int category;
  int first_action;
  u_char saved_arts;

  explicit BreakArtMenuFixture(int weapon_category)
      : category(weapon_category),
        first_action(Battle_ActionId_DaggerArtOne + (category - 1) * 4),
        saved_arts(Audio_Seq_unusedArtsStatus.artsLearned[category % 10]) {
    Menu_BreakArts_ResetMenu1Statics();
    std::fill_n(Menu_BreakArts_strings, saved_strings.size(), 0xE7E7);
    std::fill_n(Menu_BreakArts_strings, 72, 96);
    Audio_Seq_unusedArtsStatus.artsLearned[category % 10] = 4;
    Battle_characterState = Battle_actors[0] = &actor;
    actor.stats = &stats;
    actor.equippedWeaponCategory = static_cast<u_char>(category);
    actor.weaponDrawn = 1;
    stats.currentHp = 999;
    Menu_Framework_actionRestrictions = 0;
    Menu_Framework_shortcutInvoked = 0;
    for (int index = 0; index < 4; ++index) {
      auto& action = Battle_actionDefinitions[first_action + index];
      saved_actions[index] = action;
      action = {};
      action.type = Battle_ActionType_BreakArt;
      action.unlocked = 1;
      std::fill_n(action.name, std::size(action.name), static_cast<char>(0xE7));
    }
  }

  ~BreakArtMenuFixture() {
    Menu_BreakArts_ResetMenu1Statics();
    std::copy(saved_strings.begin(), saved_strings.end(),
              Menu_BreakArts_strings);
    std::copy(saved_actions.begin(), saved_actions.end(),
              &Battle_actionDefinitions[first_action]);
    Battle_characterState = saved_character;
    Battle_actors[0] = saved_actor;
    Menu_Framework_shortcutInvoked = saved_shortcut;
    Menu_Framework_actionRestrictions = saved_restrictions;
    Menu_Framework_displayActionCost = saved_cost;
    Audio_Seq_unusedArtsStatus.artsLearned[category % 10] = saved_arts;
  }

  void FinishRowAnimations() {
    for (auto& row : rows.workspace.rows) row.state = 1;
  }

  void Open() {
    REQUIRE(Menu_BreakArts_UpdateArtList(category) == 0);
    FinishRowAnimations();
    REQUIRE(Menu_BreakArts_UpdateArtList(0) == 0);
  }

  int SelectRow(int row) {
    Menu_Framework_selectedRow = row;
    Menu_Framework_CloseRowsAndRememberCursor();
    REQUIRE(Menu_BreakArts_UpdateArtList(0) == 0);
    FinishRowAnimations();
    return Menu_BreakArts_UpdateArtList(0);
  }
};

struct EquipmentGateFixture {
  CursorFixture rows;
  decltype(std::to_array(Menu_Inventory_menuText)) saved_strings =
      std::to_array(Menu_Inventory_menuText);
  decltype(Audio_Seq_unusedStateFlags) saved_flags = Audio_Seq_unusedStateFlags;
  decltype(Audio_Seq_unusedInventory) saved_inventory =
      Audio_Seq_unusedInventory;
  decltype(Audio_Seq_unusedInventoryIndices) saved_indices =
      Audio_Seq_unusedInventoryIndices;
  Battle_Actor* saved_character = Battle_characterState;
  decltype(Menu_Framework_actionRestrictions) saved_restrictions =
      Menu_Framework_actionRestrictions;
  Battle_Actor actor{};
  Battle_ActorStats stats{};

  EquipmentGateFixture() {
    Menu_Inventory_ItemList_ResetMenu3Statics();
    std::fill_n(Menu_Inventory_menuText, saved_strings.size(), 0xE7E7);
    std::fill_n(Menu_Inventory_menuText, 128, 128);
    Audio_Seq_unusedInventory = {};
    Audio_Seq_unusedInventoryIndices = {};
    actor.stats = &stats;
    Battle_characterState = &actor;
  }

  ~EquipmentGateFixture() {
    Menu_Inventory_ItemList_ResetMenu3Statics();
    std::copy(saved_strings.begin(), saved_strings.end(),
              Menu_Inventory_menuText);
    Audio_Seq_unusedStateFlags = saved_flags;
    Audio_Seq_unusedInventory = saved_inventory;
    Audio_Seq_unusedInventoryIndices = saved_indices;
    Battle_characterState = saved_character;
    Menu_Framework_actionRestrictions = saved_restrictions;
  }
};

struct WorkshopGateFixture {
  CursorFixture rows;
  decltype(Audio_Seq_unusedInventory) saved_inventory =
      Audio_Seq_unusedInventory;
  u_short* saved_text = Menu_Framework_menu12Text;
  int* saved_types = Menu_Framework_rowTypeBuf;
  Field_MpdEnvironment* saved_environment = Field_roomData.environment;
  u_char saved_workshop = Menu_Setup_currentWorkshop;
  std::array<u_short, 1024> strings{};
  Field_MpdEnvironment environment{};

  WorkshopGateFixture(int workshop, int weapon_count, bool cursor_memory) {
    Menu_Setup_SetupScreen_ResetMenucStatics();
    std::fill(strings.begin(), strings.end(), 0xE7E7);
    std::fill_n(strings.begin(), 16, 32);
    Menu_Framework_menu12Text = strings.data();
    Menu_Framework_rowTypeBuf = rows.row_types.data();
    Field_roomData.environment = &environment;
    Audio_Seq_unusedInventory = {};
    environment.isWorkshop = static_cast<char>(workshop);
    Input_settings.cursorMemory = static_cast<char>(cursor_memory);
    for (int index = 0; index < weapon_count; ++index) {
      Audio_Seq_unusedInventory.weapons[index].index =
          static_cast<char>(index + 1);
      Audio_Seq_unusedInventory.weapons[index].blade =
          static_cast<u_char>(index + 1);
    }
  }

  ~WorkshopGateFixture() {
    Menu_Setup_SetupScreen_ResetMenucStatics();
    Audio_Seq_unusedInventory = saved_inventory;
    Menu_Framework_menu12Text = saved_text;
    Menu_Framework_rowTypeBuf = saved_types;
    Field_roomData.environment = saved_environment;
    Menu_Setup_currentWorkshop = saved_workshop;
  }

  const Menu_Framework_ItemText* Open() {
    u_char state = 2;
    REQUIRE(Menu_Setup_Update(&state) == 0);
    REQUIRE(state == 3);
    REQUIRE(Menu_Framework_subMenuRowCount == 6);
    const auto* items = reinterpret_cast<const Menu_Framework_ItemText*>(
        Menu_Framework_itemsText);
    REQUIRE(items != nullptr);
    return items;
  }
};

struct WorkshopChoicesFixture {
  WorkshopGateFixture setup{6, 0, false};
  decltype(Audio_Seq_unusedInventoryIndices) saved_indices =
      Audio_Seq_unusedInventoryIndices;
  decltype(Battle_stringContext) saved_context = Battle_stringContext;
  decltype(Menu_Framework_itemNames) saved_names = Menu_Framework_itemNames;
  u_short* saved_help = Menu_Framework_itemHelp;
  decltype(Menu_Framework_shields) saved_shields = Menu_Framework_shields;
  u_short* saved_list = Menu_Setup_itemsList;
  char saved_count = Menu_Setup_itemCount;
  int saved_selection = Menu_Setup_itemsListSelection;
  char saved_state = Menu_Setup_itemsListState;
  char saved_window = Menu_Setup_itemsListWindow;
  char saved_info = Menu_Setup_rowInfoUnused;
  decltype(std::to_array(Menu_Setup_availableItems)) saved_available =
      std::to_array(Menu_Setup_availableItems);
  decltype(std::to_array(Menu_Setup_combiningBladeIds)) saved_blades =
      std::to_array(Menu_Setup_combiningBladeIds);
  decltype(std::to_array(Menu_Setup_combiningArmorIds)) saved_armor =
      std::to_array(Menu_Setup_combiningArmorIds);
  decltype(std::to_array(Menu_Setup_itemsToCombine)) saved_combining =
      std::to_array(Menu_Setup_itemsToCombine);
  std::array<u_short, 2048> strings{};
  std::array<u_short, 16384> help{};
  char names[512][24]{};
  std::array<int, 49> types{};

  WorkshopChoicesFixture() {
    std::fill(strings.begin(), strings.end(), 0xE7E7);
    std::fill_n(strings.begin(), 128, 128);
    std::fill(help.begin(), help.end(), 0xE7E7);
    std::fill_n(help.begin(), 768, 1024);
    for (auto& name : names) {
      std::fill_n(name, std::size(name), static_cast<char>(0xE7));
      name[0] = 10;
    }
    Menu_Framework_menu12Text = strings.data();
    Menu_Framework_itemHelp = help.data();
    Menu_Framework_itemNames = names;
    Menu_Framework_rowTypeBuf = types.data();
    Menu_Framework_shields = Audio_Seq_unusedInventory.shields;
    Menu_Setup_itemsList = nullptr;
    Menu_Setup_currentWorkshop = 6;
    Audio_Seq_unusedInventoryIndices = {};
    std::fill_n(Menu_Setup_combiningBladeIds, saved_blades.size(), 0);
    std::fill_n(Menu_Setup_combiningArmorIds, saved_armor.size(), 0);
    std::fill_n(Menu_Setup_itemsToCombine, saved_combining.size(), 0);
    FillParts();
    FillProtection();
    for (int category = 1; category <= 5; ++category) {
      Items_Inventory_RebuildDisplayIndices(category);
    }
  }

  ~WorkshopChoicesFixture() {
    if (Menu_Setup_itemsList != nullptr) {
      Core_Memory_FreeFromHighEnd(Menu_Setup_itemsList);
    }
    Menu_Setup_itemsList = saved_list;
    Menu_Setup_itemCount = saved_count;
    Menu_Setup_itemsListSelection = saved_selection;
    Menu_Setup_itemsListState = saved_state;
    Menu_Setup_itemsListWindow = saved_window;
    Menu_Setup_rowInfoUnused = saved_info;
    Audio_Seq_unusedInventoryIndices = saved_indices;
    Battle_stringContext = saved_context;
    Menu_Framework_itemNames = saved_names;
    Menu_Framework_itemHelp = saved_help;
    Menu_Framework_shields = saved_shields;
    std::copy(saved_available.begin(), saved_available.end(),
              Menu_Setup_availableItems);
    std::copy(saved_blades.begin(), saved_blades.end(),
              Menu_Setup_combiningBladeIds);
    std::copy(saved_armor.begin(), saved_armor.end(),
              Menu_Setup_combiningArmorIds);
    std::copy(saved_combining.begin(), saved_combining.end(),
              Menu_Setup_itemsToCombine);
  }

  void FillParts() {
    for (int index = 0; index < 16; ++index) {
      auto& blade = Audio_Seq_unusedInventory.blades[index];
      blade.id = 1;
      blade.index = static_cast<char>(index + 1);
      blade.category = blade.damageType = 1;
      blade.material = 3;
      auto& grip = Audio_Seq_unusedInventory.grips[index];
      grip.id = 97;
      grip.index = static_cast<u_short>(index + 1);
      grip.category = 1;
    }
    for (int index = 0; index < 48; ++index) {
      auto& gem = Audio_Seq_unusedInventory.gems[index];
      gem.id = 261;
      gem.index = static_cast<int16_t>(index + 1);
    }
  }

  void FillProtection() {
    for (int index = 0; index < 16; ++index) {
      auto& armor = Audio_Seq_unusedInventory.armor[index];
      armor.id = 161;
      armor.index = static_cast<u_char>(index + 1);
      armor.category = 1;
      armor.material = 3;
      armor.maxDp = 1000;
      if (index < 8) {
        auto& shield = Audio_Seq_unusedInventory.shields[index];
        shield.base.id = 132;
        shield.base.material = 3;
        shield.base.maxDp = 1000;
      }
    }
  }

  std::array<std::byte, sizeof(Items_Inventory)> InventoryBytes() const {
    std::array<std::byte, sizeof(Items_Inventory)> bytes{};
    std::memcpy(bytes.data(), &Audio_Seq_unusedInventory, bytes.size());
    return bytes;
  }

  void Open(int (*select)(int), int initial, int capacity) {
    REQUIRE(select(initial) == 0);
    for (auto& row : setup.rows.workspace.rows) row.state = 1;
    // InitializeHeap excludes its final header from the usable span.
    std::array<std::byte, sizeof(Core_HeapBlockHeader)> before{}, after{};
    std::memcpy(before.data(), &setup.rows.heap.back(), before.size());
    REQUIRE(select(0) == 0);
    std::memcpy(after.data(), &setup.rows.heap.back(), after.size());
    CHECK(before == after);
    REQUIRE(Menu_Setup_itemCount == capacity + 1);
    REQUIRE(Menu_Setup_itemsList != nullptr);
  }

  void CheckRows(int capacity) const {
    const auto* items =
        reinterpret_cast<const Menu_Framework_ItemText*>(Menu_Setup_itemsList);
    CHECK((items[0].menuRowFlags & 1) == 1);
    for (int row = 1; row <= capacity; ++row) {
      CAPTURE(row);
      CHECK(Menu_Setup_availableItems[row] == row);
      CHECK((items[row].menuRowFlags & 1) == 0);
      CHECK(items[row].name[0] == 0xE70A);
    }
  }
};
}  // namespace

TEST_CASE("Workshop selectors copy full stock-capacity component lists",
          "[menu][equipment][workshop][choices]") {
  struct Selector {
    int (*select)(int);
    int initial;
    int capacity;
  };
  constexpr std::array kSelectors{
      Selector{Menu_Setup_SelectAssemblyBlade, 0x80, 16},
      Selector{Menu_Setup_SelectAssemblyGrip, 0x80, 16},
      Selector{Menu_Setup_SelectGem, 11, 48},
      Selector{Menu_Setup_SelectCombinationBlade, 1, 16},
      Selector{Menu_Setup_SelectCombinationShield, 1, 8},
      Selector{Menu_Setup_SelectCombinationArmor, 1, 16}};
  const auto index = GENERATE(0, 1, 2, 3, 4, 5);
  CAPTURE(index);
  const auto& selector = kSelectors[index];
  WorkshopChoicesFixture fixture;
  const auto inventory = fixture.InventoryBytes();
  fixture.Open(selector.select, selector.initial, selector.capacity);
  fixture.CheckRows(selector.capacity);
  CHECK(inventory == fixture.InventoryBytes());
}

TEST_CASE("Setup actions obey original workshop and weapon-capacity gates",
          "[menu][equipment][workshop]") {
  const int workshop = GENERATE(0, 1, 2, 3, 4, 5, 6);
  const int weapon_count = GENERATE(0, 1, 7, 8);
  const bool cursor_memory = GENERATE(false, true);
  CAPTURE(workshop, weapon_count, cursor_memory);
  WorkshopGateFixture fixture(workshop, weapon_count, cursor_memory);
  REQUIRE(Items_Inventory_CountItems(0, nullptr) == weapon_count);
  const auto* rows = fixture.Open();
  REQUIRE(Menu_Setup_currentWorkshop == workshop);
  CHECK(Input_settings.cursorMemory == cursor_memory);
  for (int row = 0; row < 6; ++row) {
    CAPTURE(row);
    const bool disabled = (row == 0 && (workshop == 0 || weapon_count == 8)) ||
                          (row == 3 && weapon_count == 0) ||
                          ((row == 4 || row == 5) && workshop == 0);
    CHECK((rows[row].menuRowFlags & 1) == disabled);
  }
}

TEST_CASE("Equipment management obeys progression tarnish and action gates",
          "[menu][equipment][restrictions]") {
  const int intro = GENERATE(0, 1, 2, 3, 4);
  const bool tarnished = GENERATE(false, true);
  const int restrictions = GENERATE(0, 1, 2, 4, 7, 8, 0x80, 0x200);
  EquipmentGateFixture fixture;
  Audio_Seq_unusedStateFlags.introState = static_cast<char>(intro);
  fixture.stats.statuses = tarnished ? 0x80000 : 0;
  Menu_Framework_actionRestrictions = restrictions;
  REQUIRE(Menu_Inventory_UpdateItemList(1) == 0);
  REQUIRE(Menu_Inventory_UpdateItemList(0) == 0);
  REQUIRE(Menu_Framework_subMenuRowCount == 2);
  REQUIRE(Menu_Framework_itemsText != nullptr);
  const auto* rows = reinterpret_cast<const Menu_Framework_ItemText*>(
      Menu_Framework_itemsText);
  const bool disabled = intro < 3 || tarnished || (restrictions & 7) != 0;
  for (int row = 0; row < 2; ++row) {
    CHECK(((rows[row].menuRowFlags & 1) != 0) == disabled);
  }
}

TEST_CASE("Cursor Memory controls the actual root selection consumer",
          "[menu][options][cursor]") {
  const char enabled = GENERATE(char{0}, char{1});
  const u_short flags = GENERATE(u_short{0x3FF}, u_short{0x3F8});
  CursorFixture fixture;
  Input_settings.cursorMemory = enabled;
  Input_settings.menuFlags = flags;
  for (int visit = 0; visit < 3; ++visit) {
    Menu_Framework_submenuStates[0] = 7;
    CHECK(Menu_Framework_UpdateMainMenuSelection(2) == 0);
    CHECK(Menu_Framework_submenuStates[0] ==
          (enabled ? 7 : (flags == 0x3F8 ? 3 : 0)));
    CHECK(Input_settings.cursorMemory == enabled);
  }
}

TEST_CASE("Cursor Memory restores shared row and scroll positions",
          "[menu][options][cursor]") {
  const char enabled = GENERATE(char{0}, char{1});
  CursorFixture fixture;
  Input_settings.cursorMemory = enabled;
  Menu_Framework_menuMemory.cursorMemories[0] = 3;
  Menu_Framework_menuMemory.cursorMemories[1] = 5;
  fixture.OpenRows(20);
  REQUIRE(Menu_Framework_itemsText != nullptr);
  CHECK(Menu_Framework_HighlightedRow() == (enabled ? 8 : 0));
  Menu_Framework_selectedRow = 4;
  Menu_Framework_subMenuPage = 6;
  Menu_Framework_CloseRowsAndRememberCursor();
  CHECK(Menu_Framework_itemsText == nullptr);
  CHECK(Menu_Framework_menuMemory.cursorMemories[0] == 4);
  CHECK(Menu_Framework_menuMemory.cursorMemories[1] == 6);
  fixture.OpenRows(20);
  REQUIRE(Menu_Framework_itemsText != nullptr);
  CHECK(Menu_Framework_HighlightedRow() == (enabled ? 10 : 0));
}

TEST_CASE("Cursor Memory clamps a remembered selection after rows shrink",
          "[menu][options][cursor]") {
  CursorFixture fixture;
  Input_settings.cursorMemory = 1;
  Menu_Framework_menuMemory.cursorMemories[0] = 4;
  Menu_Framework_menuMemory.cursorMemories[1] = 6;
  fixture.OpenRows(3);
  REQUIRE(Menu_Framework_itemsText != nullptr);
  CHECK(Menu_Framework_HighlightedRow() == 0);
}

TEST_CASE("Options retain the real Puzzle Mode availability gate",
          "[menu][options][puzzle]") {
  const char disabled = GENERATE(char{0}, char{1});
  PuzzleOptionsFixture fixture;
  Audio_Seq_unusedStateFlags.puzzleModeDisabled = disabled;
  Input_settings.cursorMemory = 0;
  u_char state = 4;
  CHECK(Menu_Options_Update(&state) == 0);
  CHECK(state == 5);
  CHECK(Input_settings.cursorMemory == 0);
  REQUIRE(Menu_Framework_itemsText != nullptr);
  const auto* rows = reinterpret_cast<const Menu_Framework_ItemText*>(
      Menu_Framework_itemsText);
  for (int row = 0; row < 9; ++row) {
    CAPTURE(disabled, row);
    CHECK(rows[row].menuRowFlags ==
          (row == 6 && disabled ? Menu_Framework_RowTypeFlag_Disabled : 0));
  }
}

TEST_CASE(
    "Four learned Break Arts preserve their navigation row and cursor policy",
    "[menu][break-arts][cursor]") {
  const int category = GENERATE(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
  const int flags = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  const char shortcut = static_cast<char>(flags & 1);
  const bool equipped = (flags & 2) != 0;
  const char cursor_memory = static_cast<char>((flags >> 2) & 1);
  BreakArtMenuFixture fixture(category);
  Menu_Framework_shortcutInvoked = shortcut;
  if (!equipped) {
    fixture.actor.equippedWeaponCategory =
        static_cast<u_char>(category % 10 + 1);
  }
  Input_settings.cursorMemory = cursor_memory;
  Menu_Framework_menuMemory
      .cursorMemories[static_cast<std::size_t>(category + 11) * 2] = 3;
  fixture.Open();
  REQUIRE(Menu_Framework_itemsText != nullptr);
  REQUIRE(Menu_Framework_subMenuRowCount == (shortcut ? 4 : 5));
  REQUIRE(Menu_Framework_HighlightedRow() == (cursor_memory ? 3 : 0));
  CHECK(Input_settings.cursorMemory == cursor_memory);
  const auto* rows = reinterpret_cast<const Menu_Framework_ItemText*>(
      Menu_Framework_itemsText);
  for (int index = 0; index < 4; ++index) {
    CHECK(rows[index].menuRowFlags ==
          (equipped ? 0 : Menu_Framework_RowTypeFlag_Disabled));
  }
  if (!shortcut) {
    CHECK(rows[4].menuRowFlags == 2);
    CHECK(fixture.SelectRow(4) == 0xFFFF);
  }
}

TEST_CASE("The fourth learned Break Art remains an action in every category",
          "[menu][break-arts][cursor]") {
  const int category = GENERATE(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
  const char shortcut = GENERATE(char{0}, char{1});
  BreakArtMenuFixture fixture(category);
  Menu_Framework_shortcutInvoked = shortcut;
  fixture.Open();
  CHECK(fixture.SelectRow(3) == fixture.first_action + 3);
}
