#include <array>
#include <cstddef>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Core/Memory.h"
#include "Core/Memory/Heap.h"
#include "Input/Controller.h"
#include "Menu/Framework/Framework.h"
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
