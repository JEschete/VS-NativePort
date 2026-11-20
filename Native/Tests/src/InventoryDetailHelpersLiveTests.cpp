#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>

#include <catch2/catch_test_macros.hpp>

#include "Input/ControllerLiveState.hpp"
#include "Input/Pad.hpp"
#include "Items/InventoryLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/EquipmentDetailsLiveState.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Inventory/DetailsPoliciesLive.hpp"
#include "Menu/Inventory/Inventory.hpp"
#include "Menu/Inventory/ItemList.hpp"

namespace {
namespace inventory = VS::Menu::Inventory;
namespace fw = VS::Menu::Framework;
constexpr std::size_t kIndexedCategories = 7;
constexpr int kSelectedRow = 20;

struct LiveFixture {
  fw::MenuRow* previous_rows = VS::Menu::Framework::menuItems;
  VS::Menu::Framework::MenuMemory previous_memory =
      VS::Menu::Framework::menuMemory;
  VS::Items::InventoryContents previous_inventory = VS::Items::inventory;
  int previous_held = VS::Input::buttonsState;
  int previous_repeat = VS::Input::buttonRepeat;
  decltype(VS::Input::buttonsPressed) previous_pressed =
      VS::Input::buttonsPressed;
  int previous_preview = VS::Menu::Inventory::ashleyRenderState;
  int previous_y = VS::Menu::Inventory::ashleyYOffset;
  int previous_return = VS::Menu::Inventory::exitToBattle;
  std::uint8_t previous_title = VS::Menu::Inventory::secondaryTitleEnabled;
  std::uint8_t previous_categories =
      VS::Menu::Inventory::itemCategoryIconsEnabled;
  std::uint8_t previous_command = VS::Menu::Inventory::statusCommandState;
  std::uint8_t previous_header =
      VS::Menu::Inventory::selectedItemCategoryIconOnTop;
  char previous_pages = VS::Menu::Framework::enabledStatPages;
  char previous_page = VS::Menu::Framework::itemStatPage;
  std::array<std::uint8_t*, kIndexedCategories> previous_orders{};
  std::array<fw::MenuRow, fw::kRowCount> rows{};
  std::array<std::uint8_t, 48> order{};

  LiveFixture() {
    for (std::size_t i = 0; i < previous_orders.size(); ++i) {
      previous_orders[i] = VS::Menu::Inventory::categoryOrders[i];
    }
    VS::Menu::Framework::menuItems = rows.data();
    VS::Items::inventory = {};
    VS::Input::buttonsState = 0;
    VS::Input::buttonRepeat = 0;
    VS::Input::buttonsPressed = 0;
    VS::Menu::Inventory::ashleyRenderState = 0;
    VS::Menu::Inventory::ashleyYOffset = 5;
    VS::Menu::Inventory::secondaryTitleEnabled = 7;
    VS::Menu::Inventory::itemCategoryIconsEnabled = 8;
    VS::Menu::Inventory::statusCommandState = 9;
    fw::ResetLiveEquipmentDetailsStatics();
  }

  ~LiveFixture() {
    VS::Menu::Framework::menuItems = previous_rows;
    VS::Menu::Framework::menuMemory = previous_memory;
    VS::Items::inventory = previous_inventory;
    VS::Input::buttonsState = previous_held;
    VS::Input::buttonRepeat = previous_repeat;
    VS::Input::buttonsPressed = previous_pressed;
    VS::Menu::Inventory::ashleyRenderState = previous_preview;
    VS::Menu::Inventory::ashleyYOffset = previous_y;
    VS::Menu::Inventory::exitToBattle = previous_return;
    VS::Menu::Inventory::secondaryTitleEnabled = previous_title;
    VS::Menu::Inventory::itemCategoryIconsEnabled = previous_categories;
    VS::Menu::Inventory::statusCommandState = previous_command;
    VS::Menu::Inventory::selectedItemCategoryIconOnTop = previous_header;
    fw::ResetLiveEquipmentDetailsStatics();
    VS::Menu::Framework::enabledStatPages = previous_pages;
    VS::Menu::Framework::itemStatPage = previous_page;
    for (std::size_t i = 0; i < previous_orders.size(); ++i) {
      VS::Menu::Inventory::categoryOrders[i] = previous_orders[i];
    }
  }
};

auto MemoryBytes(const VS::Menu::Framework::MenuMemory& memory) {
  return std::bit_cast<
      std::array<std::uint8_t, sizeof(VS::Menu::Framework::MenuMemory)>>(
      memory);
}
}  // namespace

TEST_CASE("F02566 live panels need no row or actor owner",
          "[inventory-detail-helpers-live][F02566]") {
  LiveFixture fixture;
  VS::Menu::Framework::menuItems = nullptr;
  inventory::SetLivePanelVisibility(static_cast<char>(0xFF));
  CHECK(VS::Menu::Inventory::secondaryTitleEnabled == 0xFF);
  CHECK(VS::Menu::Inventory::itemCategoryIconsEnabled == 0xFF);
  CHECK(VS::Menu::Inventory::statusCommandState == 0xFF);
  CHECK(VS::Menu::Inventory::ashleyYOffset == 5);
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
  fixture.rows[3].state = fw::RowState::kIdle;
  fixture.rows[32].state = fw::RowState::kIdle;
  char description[] = "old description";
  fixture.rows[kSelectedRow].sub_text = description;
  inventory::BeginLiveDetailsTransition(kSelectedRow);
  CHECK(fixture.rows[3].target0 == 320);
  CHECK(fixture.rows[32].state == fw::RowState::kIdle);
  CHECK(fixture.rows[31].state == fw::RowState::kSlideY);
  CHECK(fixture.rows[31].target0 == 18);
  CHECK(fixture.rows[kSelectedRow].state == fw::RowState::kSlideX);
  CHECK(fixture.rows[kSelectedRow].target0 == 155);
  CHECK(fixture.rows[kSelectedRow].selected == 1);
  CHECK(fixture.rows[kSelectedRow].sub_text == nullptr);
}

TEST_CASE("F02571 live layout uses real transition and stat page helpers",
          "[inventory-detail-helpers-live][F02571]") {
  LiveFixture fixture;
  inventory::InitializeLiveDetailsLayout(kSelectedRow, 7);
  CHECK(VS::Menu::Inventory::ashleyRenderState == 1);
  CHECK(VS::Menu::Inventory::ashleyYOffset == 5);
  CHECK(VS::Menu::Inventory::secondaryTitleEnabled == 7);
  CHECK(VS::Menu::Inventory::itemCategoryIconsEnabled == 0);
  CHECK(VS::Menu::Inventory::statusCommandState == 0);
  CHECK(fixture.rows[31].w == 164);
  CHECK(fixture.rows[31].target0 == 16);
  CHECK(fixture.rows[kSelectedRow].target0 == 18);
  CHECK(VS::Menu::Framework::enabledStatPages == 7);
}

TEST_CASE("F02573 live selection preserves the full cursor record and peers",
          "[inventory-detail-helpers-live][F02573]") {
  LiveFixture fixture;
  std::array<std::uint8_t, sizeof(VS::Menu::Framework::MenuMemory)> initial{};
  initial.fill(0xA5);
  VS::Menu::Framework::menuMemory =
      std::bit_cast<VS::Menu::Framework::MenuMemory>(initial);
  VS::Menu::Inventory::categoryOrders[5] = fixture.order.data();
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
  fixture.rows[31].state = fw::RowState::kIdle;
  fixture.rows[31].w = 164;
  VS::Input::buttonsPressed = VS::Input::Triangle;
  inventory::ExitLiveDetailsView(1);
  CHECK(fixture.rows[31].state == fw::RowState::kDismiss);
  CHECK(fixture.rows[31].target0 == -164);
  CHECK(VS::Menu::Inventory::selectedItemCategoryIconOnTop == 2);
  CHECK(VS::Menu::Inventory::exitToBattle ==
        static_cast<int>(VS::Input::Triangle));
  VS::Input::buttonsPressed = VS::Input::Circle;
  inventory::ExitLiveDetailsView(0);
  CHECK(VS::Menu::Inventory::exitToBattle == 0);
}
