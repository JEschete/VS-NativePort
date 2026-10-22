#include <algorithm>
#include <array>
#include <cstddef>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Abilities.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Core/Memory.h"
#include "Core/Memory/Heap.h"
#include "Input/Controller.h"
#include "Menu/BreakArts.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/MenuRows.h"
#include "Menu/Options.h"
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
  std::array<Core_HeapBlockHeader, 256> heap{};
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
}  // namespace

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
