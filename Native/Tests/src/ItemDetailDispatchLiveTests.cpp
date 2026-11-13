#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "Input/Pad.hpp"
#include "Menu/ItemDetailDispatchLiveFixture.hpp"

namespace {
namespace inv = VS::Menu::Inventory;
namespace fw = VS::Menu::Framework;
using item_detail_tests::DispatchLiveFixture;
constexpr int kGemDispatchEntry = (((20 << 8) | 1) << 4) | 5;
}  // namespace

TEST_CASE("F02568 production renderer merges only the selected category icon",
          "[item-detail-dispatch-live][F02568]") {
  DispatchLiveFixture fixture;
  Menu_Framework_menuMemory.opaque3A.currentCategory = 1;
  fixture.rows[31].x = 60;
  inv::DrawLiveItemCategoryIcons(2, 1);
  const auto sprites = fixture.Sprites();
  CHECK(sprites.size() == 7);
  CHECK(fixture.rows[31].rowIcon == 24);
  CHECK(fixture.rows[31].selected == 0);
}

TEST_CASE("F02568 native sprite queues preserve overlay position and palette",
          "[item-detail-dispatch-live][F02568]") {
  DispatchLiveFixture fixture;
  Menu_Framework_menuMemory.opaque3A.currentCategory = 1;
  fixture.rows[31].x = 61;
  inv::DrawLiveItemCategoryIcons(0, 1);
  const auto sprites = fixture.Sprites();
  REQUIRE(sprites.size() == 8);
  CHECK(sprites.front().position == fw::PackXy(32, -16));
  CHECK(sprites.front().texture == 0x37FE80E8);
  CHECK(sprites.back().position == fw::PackXy(48, 16));
  CHECK(sprites.back().texture == 0x37FD8078);
}

TEST_CASE("F02579 armor details use production stats and native row layout",
          "[item-detail-dispatch-live][F02579]") {
  DispatchLiveFixture fixture;
  fixture.AddArmor();
  fixture.Open(inv::UpdateLiveArmorDetails);
  CHECK(Menu_Framework_equipmentSubtype == fw::equipment_subtype::kArmor);
  CHECK(Menu_Framework_currentStatusViewItem == 16);
  CHECK(Menu_Framework_enabledStatPages == 7);
  CHECK(Menu_Framework_strIntAgi[0] == -7);
  CHECK(fixture.rows[31].targetPosition0 == 16);
  CHECK(fixture.rows[20].targetPosition0 == 18);
}

TEST_CASE("F02579 accessory details keep their real subtype",
          "[item-detail-dispatch-live][F02579]") {
  DispatchLiveFixture fixture;
  fixture.AddArmor(true);
  fixture.Open(inv::UpdateLiveArmorDetails);
  CHECK(Menu_Framework_equipmentSubtype == fw::equipment_subtype::kAccessory);
  CHECK(Menu_Framework_currentStatusViewItem == 16);
  CHECK(Menu_Framework_enabledStatPages == 7);
}

TEST_CASE("F02579 paging integrates the real armor formatter and cursor memory",
          "[item-detail-dispatch-live][F02579]") {
  DispatchLiveFixture fixture;
  fixture.AddArmor();
  fixture.Open(inv::UpdateLiveArmorDetails);
  fixture.Wait(inv::UpdateLiveArmorDetails);
  Input_buttonRepeat = VS::Input::R1;
  CHECK(inv::UpdateLiveArmorDetails(0) == 0);
  CHECK(Menu_Framework_currentStatusViewItem == 1);
  CHECK(Menu_Framework_menuMemory.opaque3A.categoryCursors[4].position.page ==
        1);
  CHECK(fixture.rows[31].itemPage == 2);
  CHECK(fixture.rows[20].selected == 1);
}

TEST_CASE(
    "F02580 gem details use production signed stats and class affinity pages",
    "[item-detail-dispatch-live][F02580]") {
  DispatchLiveFixture fixture;
  fixture.AddGems();
  fixture.Open(inv::UpdateLiveGemDetails);
  CHECK(Menu_Framework_equipmentSubtype == fw::equipment_subtype::kGem);
  CHECK(Menu_Framework_currentStatusViewItem == 48);
  CHECK(Menu_Framework_enabledStatPages == 3);
  CHECK(Menu_Framework_equipmentStats[0] == -4);
  CHECK(Menu_Framework_equipmentStats[16] == 7);
  CHECK(Menu_Framework_strIntAgi[0] == 9);
}

TEST_CASE(
    "F02580 paging uses real gem text and stores its one-based title page",
    "[item-detail-dispatch-live][F02580]") {
  DispatchLiveFixture fixture;
  fixture.AddGems();
  fixture.Open(inv::UpdateLiveGemDetails);
  fixture.Wait(inv::UpdateLiveGemDetails);
  Input_buttonRepeat = VS::Input::R1;
  CHECK(inv::UpdateLiveGemDetails(0) == 0);
  CHECK(Menu_Framework_currentStatusViewItem == 1);
  CHECK(Menu_Framework_menuMemory.opaque3A.categoryCursors[5].position.page ==
        1);
  CHECK(fixture.rows[31].itemPage == 2);
  CHECK(Menu_Framework_strIntAgi[0] == 5);
}

TEST_CASE("F02581 packed list selection enters the real gem controller",
          "[item-detail-dispatch-live][F02581]") {
  DispatchLiveFixture fixture;
  fixture.AddGems();
  CHECK(inv::UpdateLiveItemDetails(kGemDispatchEntry) == 0);
  CHECK(fixture.rows[31].itemPage == 1);
  CHECK(Menu_Framework_selectedStatusViewElement == 9);
  CHECK(Menu_Inventory_exitToBattle == 0);
  fixture.IdleRows();
  CHECK(inv::UpdateLiveItemDetails(0) == 0);
  CHECK(Menu_Framework_currentStatusViewItem == 48);
}

TEST_CASE(
    "F02581 ready details draw through the real framework cursor and font",
    "[item-detail-dispatch-live][F02581]") {
  DispatchLiveFixture fixture;
  fixture.AddGems();
  CHECK(inv::UpdateLiveItemDetails(kGemDispatchEntry) == 0);
  fixture.IdleRows();
  CHECK(inv::UpdateLiveItemDetails(0) == 0);
  fixture.IdleRows();
  fixture.ResetFrame();
  CHECK(inv::UpdateLiveItemDetails(0) == 0);
  CHECK(Menu_Framework_cursorColor == 0);
  CHECK_FALSE(fixture.Sprites().empty());
}

TEST_CASE(
    "F02581 category-only continuation preserves an existing controller page",
    "[item-detail-dispatch-live][F02581]") {
  DispatchLiveFixture fixture;
  fixture.AddGems();
  CHECK(inv::UpdateLiveGemDetails((20 << 8) | 2) == 0);
  fixture.IdleRows();
  CHECK(inv::UpdateLiveGemDetails(0) == 0);
  CHECK(inv::UpdateLiveItemDetails(5) == 0);
  CHECK(Menu_Framework_currentStatusViewItem == 1);
  CHECK(fixture.rows[31].itemPage == 0);
}

TEST_CASE("F02581 normal close restores list commands and Ashley presentation",
          "[item-detail-dispatch-live][F02581]") {
  DispatchLiveFixture fixture;
  fixture.AddGems();
  CHECK(inv::UpdateLiveItemDetails(kGemDispatchEntry) == 0);
  fixture.IdleRows();
  CHECK(inv::UpdateLiveItemDetails(0) == 0);
  fixture.Wait(inv::UpdateLiveGemDetails);
  Input_buttonsPressed.allButtons = VS::Input::Cross;
  CHECK(inv::UpdateLiveItemDetails(0) == 0);
  Input_buttonsPressed.allButtons = 0;
  fixture.IdleRows();
  CHECK(inv::UpdateLiveItemDetails(0) == 1);
  CHECK(Menu_Inventory_itemCategoryIconsEnabled == 1);
  CHECK(Menu_Inventory_selectedItemCategoryIconOnTop == 0);
  CHECK(Menu_Inventory_statusCommandState == 1);
  CHECK(Menu_Inventory_ashleyRenderState == 0);
}

TEST_CASE("F02581 battle close consumes the actual triangle return intent",
          "[item-detail-dispatch-live][F02581]") {
  DispatchLiveFixture fixture;
  fixture.AddGems();
  CHECK(inv::UpdateLiveItemDetails(kGemDispatchEntry) == 0);
  fixture.IdleRows();
  CHECK(inv::UpdateLiveItemDetails(0) == 0);
  fixture.Wait(inv::UpdateLiveGemDetails);
  Input_buttonsPressed.allButtons = VS::Input::Triangle;
  CHECK(inv::UpdateLiveItemDetails(0) == 0);
  Input_buttonsPressed.allButtons = 0;
  fixture.IdleRows();
  CHECK(inv::UpdateLiveItemDetails(0) == -2);
  CHECK(Menu_Inventory_exitToBattle == static_cast<int>(VS::Input::Triangle));
}
