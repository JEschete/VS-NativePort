#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>

#include <catch2/catch_test_macros.hpp>

#include "Input/Pad.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Inventory/DetailsPoliciesLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemList.h"
}

namespace {
namespace inventory = VS::Menu::Inventory;
namespace fw = VS::Menu::Framework;
constexpr std::size_t kIndexedCategories = 7;
constexpr int kSelectedRow = 20;

struct LiveFixture {
  Menu_Framework_MenuRow* previous_rows = Menu_Framework_menuItems;
  Menu_Framework_MenuMemory previous_memory = Menu_Framework_menuMemory;
  Items_Inventory previous_inventory = Audio_Seq_unusedInventory;
  int previous_held = Input_buttonsState;
  int previous_repeat = Input_buttonRepeat;
  decltype(Input_buttonsPressed) previous_pressed = Input_buttonsPressed;
  int previous_preview = Menu_Inventory_ashleyRenderState;
  int previous_y = Menu_Inventory_ashleyYOffset;
  int previous_return = Menu_Inventory_exitToBattle;
  char previous_title = Menu_Inventory_secondaryTitleEnabled;
  char previous_categories = Menu_Inventory_itemCategoryIconsEnabled;
  char previous_command = Menu_Inventory_statusCommandState;
  char previous_header = Menu_Inventory_selectedItemCategoryIconOnTop;
  char previous_pages = Menu_Framework_enabledStatPages;
  char previous_page = Menu_Framework_itemStatPage;
  std::array<u_char*, kIndexedCategories> previous_orders{};
  std::array<Menu_Framework_MenuRow, fw::kRowCount> rows{};
  std::array<std::uint8_t, 48> order{};

  LiveFixture() {
    for (std::size_t i = 0; i < previous_orders.size(); ++i) {
      previous_orders[i] = Menu_Inventory_inventoryIndices[i];
    }
    Menu_Framework_menuItems = rows.data();
    Audio_Seq_unusedInventory = {};
    Input_buttonsState = 0;
    Input_buttonRepeat = 0;
    Input_buttonsPressed.allButtons = 0;
    Menu_Inventory_ashleyRenderState = 0;
    Menu_Inventory_ashleyYOffset = 5;
    Menu_Inventory_secondaryTitleEnabled = 7;
    Menu_Inventory_itemCategoryIconsEnabled = 8;
    Menu_Inventory_statusCommandState = 9;
    fw::ResetLiveEquipmentDetailsStatics();
  }

  ~LiveFixture() {
    Menu_Framework_menuItems = previous_rows;
    Menu_Framework_menuMemory = previous_memory;
    Audio_Seq_unusedInventory = previous_inventory;
    Input_buttonsState = previous_held;
    Input_buttonRepeat = previous_repeat;
    Input_buttonsPressed = previous_pressed;
    Menu_Inventory_ashleyRenderState = previous_preview;
    Menu_Inventory_ashleyYOffset = previous_y;
    Menu_Inventory_exitToBattle = previous_return;
    Menu_Inventory_secondaryTitleEnabled = previous_title;
    Menu_Inventory_itemCategoryIconsEnabled = previous_categories;
    Menu_Inventory_statusCommandState = previous_command;
    Menu_Inventory_selectedItemCategoryIconOnTop = previous_header;
    fw::ResetLiveEquipmentDetailsStatics();
    Menu_Framework_enabledStatPages = previous_pages;
    Menu_Framework_itemStatPage = previous_page;
    for (std::size_t i = 0; i < previous_orders.size(); ++i) {
      Menu_Inventory_inventoryIndices[i] = previous_orders[i];
    }
  }
};

auto MemoryBytes(const Menu_Framework_MenuMemory& memory) {
  return std::bit_cast<
      std::array<std::uint8_t, sizeof(Menu_Framework_MenuMemory)>>(memory);
}
}  // namespace

TEST_CASE("F02566 live panels need no row or actor owner",
          "[inventory-detail-helpers-live][F02566]") {
  LiveFixture fixture;
  Menu_Framework_menuItems = nullptr;
  inventory::SetLivePanelVisibility(static_cast<char>(0xFF));
  CHECK(static_cast<std::uint8_t>(Menu_Inventory_secondaryTitleEnabled) ==
        0xFF);
  CHECK(static_cast<std::uint8_t>(Menu_Inventory_itemCategoryIconsEnabled) ==
        0xFF);
  CHECK(static_cast<std::uint8_t>(Menu_Inventory_statusCommandState) == 0xFF);
  CHECK(Menu_Inventory_ashleyYOffset == 5);
}

TEST_CASE("F02567 live paging uses the production inventory count lazily",
          "[inventory-detail-helpers-live][F02567]") {
  LiveFixture fixture;
  Audio_Seq_unusedInventory.weapons[0].blade = 1;
  Audio_Seq_unusedInventory.weapons[7].blade = 16;
  Input_buttonRepeat = VS::Input::L1;
  CHECK(inventory::PollLiveDetailsPaging(0, 0) == 1);
  Input_buttonRepeat = VS::Input::R1;
  CHECK(inventory::PollLiveDetailsPaging(0, 1) == 0);
  Input_buttonsState = VS::Input::L1 | VS::Input::R1;
  Menu_Framework_menuItems = nullptr;
  CHECK(inventory::PollLiveDetailsPaging(-1, 17) == 17);
}

TEST_CASE("F02569 live transition integrates real framework row services",
          "[inventory-detail-helpers-live][F02569]") {
  LiveFixture fixture;
  fixture.rows[3].state = Menu_Framework_MenuItemState_Idle;
  fixture.rows[32].state = Menu_Framework_MenuItemState_Idle;
  char description[] = "old description";
  fixture.rows[kSelectedRow].subText = description;
  inventory::BeginLiveDetailsTransition(kSelectedRow);
  CHECK(fixture.rows[3].targetPosition0 == 320);
  CHECK(fixture.rows[32].state == Menu_Framework_MenuItemState_Idle);
  CHECK(fixture.rows[31].state == Menu_Framework_MenuItemState_SlideY);
  CHECK(fixture.rows[31].targetPosition0 == 18);
  CHECK(fixture.rows[kSelectedRow].state ==
        Menu_Framework_MenuItemState_SlideX);
  CHECK(fixture.rows[kSelectedRow].targetPosition0 == 155);
  CHECK(fixture.rows[kSelectedRow].selected == 1);
  CHECK(fixture.rows[kSelectedRow].subText == nullptr);
}

TEST_CASE("F02571 live layout uses real transition and stat page helpers",
          "[inventory-detail-helpers-live][F02571]") {
  LiveFixture fixture;
  inventory::InitializeLiveDetailsLayout(kSelectedRow, 7);
  CHECK(Menu_Inventory_ashleyRenderState == 1);
  CHECK(Menu_Inventory_ashleyYOffset == 5);
  CHECK(Menu_Inventory_secondaryTitleEnabled == 7);
  CHECK(Menu_Inventory_itemCategoryIconsEnabled == 0);
  CHECK(Menu_Inventory_statusCommandState == 0);
  CHECK(fixture.rows[31].w == 164);
  CHECK(fixture.rows[31].targetPosition0 == 16);
  CHECK(fixture.rows[kSelectedRow].targetPosition0 == 18);
  CHECK(Menu_Framework_enabledStatPages == 7);
}

TEST_CASE("F02573 live selection preserves the full cursor record and peers",
          "[inventory-detail-helpers-live][F02573]") {
  LiveFixture fixture;
  std::array<std::uint8_t, sizeof(Menu_Framework_MenuMemory)> initial{};
  initial.fill(0xA5);
  Menu_Framework_menuMemory = std::bit_cast<Menu_Framework_MenuMemory>(initial);
  Menu_Inventory_inventoryIndices[5] = fixture.order.data();
  fixture.order[47] = 48;
  auto expected = initial;
  expected[0x3C + 5 * 2] = 0;
  expected[0x3C + 5 * 2 + 1] = 47;
  CHECK(inventory::SelectLiveDetailsItem(5, 47) == 48);
  CHECK(MemoryBytes(Menu_Framework_menuMemory) == expected);
  fixture.order[47] = 1;
  CHECK(inventory::SelectLiveDetailsItem(5, 47) == 1);
}

TEST_CASE("F02574 live close integrates rows and retains the source exit mask",
          "[inventory-detail-helpers-live][F02574]") {
  LiveFixture fixture;
  fixture.rows[31].state = Menu_Framework_MenuItemState_Idle;
  fixture.rows[31].w = 164;
  Input_buttonsPressed.allButtons = VS::Input::Triangle;
  inventory::ExitLiveDetailsView(1);
  CHECK(fixture.rows[31].state == Menu_Framework_MenuItemState_Dismiss);
  CHECK(fixture.rows[31].targetPosition0 == -164);
  CHECK(Menu_Inventory_selectedItemCategoryIconOnTop == 2);
  CHECK(Menu_Inventory_exitToBattle == static_cast<int>(VS::Input::Triangle));
  Input_buttonsPressed.allButtons = VS::Input::Circle;
  inventory::ExitLiveDetailsView(0);
  CHECK(Menu_Inventory_exitToBattle == 0);
}
