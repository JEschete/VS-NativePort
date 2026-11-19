#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>

#include <catch2/catch_test_macros.hpp>

#include "Input/ControllerLiveState.hpp"
#include "Input/Pad.hpp"
#include "Items/Inventory.h"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/EquipmentDetailsLiveState.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Inventory/DetailsPoliciesLive.hpp"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemList.h"

namespace {
namespace inventory = VS::Menu::Inventory;
namespace fw = VS::Menu::Framework;
constexpr std::size_t kIndexedCategories = 7;
constexpr int kSelectedRow = 20;

struct LiveFixture {
  Menu_Framework_MenuRow* previous_rows = VS::Menu::Framework::menuItems;
  Menu_Framework_MenuMemory previous_memory = VS::Menu::Framework::menuMemory;
  Items_Inventory previous_inventory = VS::Items::inventory;
  int previous_held = VS::Input::buttonsState;
  int previous_repeat = VS::Input::buttonRepeat;
  decltype(VS::Input::buttonsPressed) previous_pressed =
      VS::Input::buttonsPressed;
  int previous_preview = Menu_Inventory_ashleyRenderState;
  int previous_y = Menu_Inventory_ashleyYOffset;
  int previous_return = Menu_Inventory_exitToBattle;
  char previous_title = Menu_Inventory_secondaryTitleEnabled;
  char previous_categories = Menu_Inventory_itemCategoryIconsEnabled;
  char previous_command = Menu_Inventory_statusCommandState;
  char previous_header = Menu_Inventory_selectedItemCategoryIconOnTop;
  char previous_pages = VS::Menu::Framework::enabledStatPages;
  char previous_page = VS::Menu::Framework::itemStatPage;
  std::array<u_char*, kIndexedCategories> previous_orders{};
  std::array<Menu_Framework_MenuRow, fw::kRowCount> rows{};
  std::array<std::uint8_t, 48> order{};

  LiveFixture() {
    for (std::size_t i = 0; i < previous_orders.size(); ++i) {
      previous_orders[i] = Menu_Inventory_inventoryIndices[i];
    }
    VS::Menu::Framework::menuItems = rows.data();
    VS::Items::inventory = {};
    VS::Input::buttonsState = 0;
    VS::Input::buttonRepeat = 0;
    VS::Input::buttonsPressed = 0;
    Menu_Inventory_ashleyRenderState = 0;
    Menu_Inventory_ashleyYOffset = 5;
    Menu_Inventory_secondaryTitleEnabled = 7;
    Menu_Inventory_itemCategoryIconsEnabled = 8;
    Menu_Inventory_statusCommandState = 9;
    fw::ResetLiveEquipmentDetailsStatics();
  }

  ~LiveFixture() {
    VS::Menu::Framework::menuItems = previous_rows;
    VS::Menu::Framework::menuMemory = previous_memory;
    VS::Items::inventory = previous_inventory;
    VS::Input::buttonsState = previous_held;
    VS::Input::buttonRepeat = previous_repeat;
    VS::Input::buttonsPressed = previous_pressed;
    Menu_Inventory_ashleyRenderState = previous_preview;
    Menu_Inventory_ashleyYOffset = previous_y;
    Menu_Inventory_exitToBattle = previous_return;
    Menu_Inventory_secondaryTitleEnabled = previous_title;
    Menu_Inventory_itemCategoryIconsEnabled = previous_categories;
    Menu_Inventory_statusCommandState = previous_command;
    Menu_Inventory_selectedItemCategoryIconOnTop = previous_header;
    fw::ResetLiveEquipmentDetailsStatics();
    VS::Menu::Framework::enabledStatPages = previous_pages;
    VS::Menu::Framework::itemStatPage = previous_page;
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
  VS::Menu::Framework::menuItems = nullptr;
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
  VS::Items::inventory.weapons[0].blade = 1;
  VS::Items::inventory.weapons[7].blade = 16;
  VS::Input::buttonRepeat = VS::Input::L1;
  CHECK(inventory::PollLiveDetailsPaging(0, 0) == 1);
  VS::Input::buttonRepeat = VS::Input::R1;
  CHECK(inventory::PollLiveDetailsPaging(0, 1) == 0);
  VS::Input::buttonsState = VS::Input::L1 | VS::Input::R1;
  VS::Menu::Framework::menuItems = nullptr;
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
  CHECK(VS::Menu::Framework::enabledStatPages == 7);
}

TEST_CASE("F02573 live selection preserves the full cursor record and peers",
          "[inventory-detail-helpers-live][F02573]") {
  LiveFixture fixture;
  std::array<std::uint8_t, sizeof(Menu_Framework_MenuMemory)> initial{};
  initial.fill(0xA5);
  VS::Menu::Framework::menuMemory =
      std::bit_cast<Menu_Framework_MenuMemory>(initial);
  Menu_Inventory_inventoryIndices[5] = fixture.order.data();
  fixture.order[47] = 48;
  auto expected = initial;
  expected[0x3C + 5 * 2] = 0;
  expected[0x3C + 5 * 2 + 1] = 47;
  CHECK(inventory::SelectLiveDetailsItem(5, 47) == 48);
  CHECK(MemoryBytes(VS::Menu::Framework::menuMemory) == expected);
  fixture.order[47] = 1;
  CHECK(inventory::SelectLiveDetailsItem(5, 47) == 1);
}

TEST_CASE("F02574 live close integrates rows and retains the source exit mask",
          "[inventory-detail-helpers-live][F02574]") {
  LiveFixture fixture;
  fixture.rows[31].state = Menu_Framework_MenuItemState_Idle;
  fixture.rows[31].w = 164;
  VS::Input::buttonsPressed = VS::Input::Triangle;
  inventory::ExitLiveDetailsView(1);
  CHECK(fixture.rows[31].state == Menu_Framework_MenuItemState_Dismiss);
  CHECK(fixture.rows[31].targetPosition0 == -164);
  CHECK(Menu_Inventory_selectedItemCategoryIconOnTop == 2);
  CHECK(Menu_Inventory_exitToBattle == static_cast<int>(VS::Input::Triangle));
  VS::Input::buttonsPressed = VS::Input::Circle;
  inventory::ExitLiveDetailsView(0);
  CHECK(Menu_Inventory_exitToBattle == 0);
}
