#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "Input/Pad.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"

namespace {
namespace inventory = VS::Menu::Inventory;
namespace fw = VS::Menu::Framework;
using equipment_detail_tests::LiveFixture;
constexpr int kShieldGemRows = static_cast<int>(VS::Items::kWeaponGemCount);
constexpr int kWeaponDetailRows = kShieldGemRows + 2;
}  // namespace

TEST_CASE("F02575 live initialization uses production weapon stats and layout",
          "[equipment-detail-controllers-live][F02575]") {
  LiveFixture fixture;
  fixture.AddWeapon();
  fixture.Open(inventory::UpdateLiveWeaponDetails);
  CHECK(Menu_Framework_equipmentSubtype == 1);
  CHECK(Menu_Framework_currentStatusViewItem == 1);
  CHECK(Menu_Framework_enabledStatPages == 7);
  CHECK(fixture.rows[31].targetPosition0 == 16);
  CHECK(fixture.rows[20].targetPosition0 == 18);
  CHECK(Menu_Inventory_ashleyYOffset == 5);
}

TEST_CASE(
    "F02575 live component rows use real blade grip and absent gem helpers",
    "[equipment-detail-controllers-live][F02575]") {
  LiveFixture fixture;
  fixture.AddWeapon();
  fixture.Open(inventory::UpdateLiveWeaponDetails);
  fixture.Wait(inventory::UpdateLiveWeaponDetails, kWeaponDetailRows);
  CHECK(fixture.rows[11].state == Menu_Framework_MenuItemState_SlideX);
  CHECK(fixture.rows[12].state == Menu_Framework_MenuItemState_SlideX);
  CHECK(fixture.rows[13].unselectable == 1);
  CHECK(fixture.rows[14].unselectable == 1);
  CHECK(fixture.rows[15].state == Menu_Framework_MenuItemState_Inactive);
  CHECK(fixture.rows[13].targetPosition0 == 169);
}

TEST_CASE("F02576 live blade stats retain sign and class affinity-only pages",
          "[equipment-detail-controllers-live][F02576]") {
  LiveFixture fixture;
  fixture.AddParts();
  fixture.Open(inventory::UpdateLiveBladeDetails);
  CHECK(Menu_Framework_currentStatusViewItem == 16);
  CHECK(Menu_Framework_enabledStatPages == 3);
  CHECK(Menu_Framework_equipmentSubtype == 2);
  CHECK(Menu_Framework_equipmentStats[0] == -3);
  CHECK(Menu_Framework_equipmentStats[16] == 4);
  CHECK(Menu_Framework_strIntAgi[0] == 9);
}

TEST_CASE(
    "F02576 live paging integrates real formatter selection and cursor memory",
    "[equipment-detail-controllers-live][F02576]") {
  LiveFixture fixture;
  fixture.AddParts();
  fixture.Open(inventory::UpdateLiveBladeDetails);
  fixture.Wait(inventory::UpdateLiveBladeDetails);
  Input_buttonRepeat = VS::Input::R1;
  CHECK(inventory::UpdateLiveBladeDetails(0) == 0);
  CHECK(Menu_Framework_currentStatusViewItem == 1);
  CHECK(Menu_Framework_menuMemory.opaque3A.categoryCursors[1].position.page ==
        1);
  CHECK(fixture.rows[31].itemPage == 2);
  CHECK(fixture.rows[20].selected == 1);
  CHECK(Menu_Framework_strIntAgi[0] == 7);
}

TEST_CASE("F02577 live grip stats are type-only without requiring gem owners",
          "[equipment-detail-controllers-live][F02577]") {
  LiveFixture fixture;
  fixture.AddParts();
  Menu_Framework_gems = nullptr;
  fixture.Open(inventory::UpdateLiveGripDetails);
  CHECK(Menu_Framework_equipmentSubtype == 4);
  CHECK(Menu_Framework_enabledStatPages == 4);
  CHECK(Menu_Framework_equipmentStats[33] == -2);
  CHECK(Menu_Framework_strIntAgi[0] == 5);
  CHECK(Menu_Framework_currentStatusViewItem == 1);
}

TEST_CASE(
    "F02577 production icon and compact font show mutable current slot count",
    "[equipment-detail-controllers-live][F02577]") {
  LiveFixture fixture;
  fixture.AddParts();
  fixture.Open(inventory::UpdateLiveGripDetails);
  fixture.Wait(inventory::UpdateLiveGripDetails);
  fixture.ResetFrame();
  CHECK(inventory::UpdateLiveGripDetails(0) == 0);
  const auto actual = fixture.Sprites();
  fixture.ResetFrame();
  fw::DrawLiveRowIcon(equipment_detail_tests::kCountIcon,
                      equipment_detail_tests::kCountIconX,
                      equipment_detail_tests::kCountIconY);
  VS::Render::DrawLiveCompactText(
      "X     2",
      static_cast<int>(fw::PackXy(equipment_detail_tests::kCountTextX,
                                  equipment_detail_tests::kCountTextY)),
      nullptr);
  const auto expected = fixture.Sprites();
  REQUIRE(expected.size() >= 3);
  CHECK(actual == expected);
  CHECK(fixture.CountLabel(actual) == "X     2");
  CHECK(Menu_Inventory_selectedItemCategoryIconOnTop == 1);
  Audio_Seq_unusedInventory.grips[0].gemSlots = VS::Items::kWeaponGemCount;
  fixture.ResetFrame();
  CHECK(inventory::UpdateLiveGripDetails(0) == 0);
  CHECK(fixture.CountLabel(fixture.Sprites()) == "X     3");
}

TEST_CASE("F02577 exit draws current grip count through actual presentation",
          "[equipment-detail-controllers-live][F02577]") {
  LiveFixture fixture;
  fixture.AddParts();
  fixture.Open(inventory::UpdateLiveGripDetails);
  fixture.Wait(inventory::UpdateLiveGripDetails);
  fixture.ResetFrame();
  Input_buttonsPressed.allButtons = VS::Input::Triangle;
  CHECK(inventory::UpdateLiveGripDetails(0) == 0);
  CHECK(Menu_Inventory_exitToBattle == static_cast<int>(VS::Input::Triangle));
  CHECK(Menu_Inventory_selectedItemCategoryIconOnTop == 2);
  const auto actual = fixture.Sprites();
  CHECK(actual.size() >= 3);
  CHECK(fixture.CountLabel(actual) == "X     2");
}

TEST_CASE("F02578 live shield initializes exactly three gem rows",
          "[equipment-detail-controllers-live][F02578]") {
  LiveFixture fixture;
  fixture.AddShield();
  fixture.Open(inventory::UpdateLiveShieldDetails);
  fixture.Wait(inventory::UpdateLiveShieldDetails, kShieldGemRows);
  CHECK(Menu_Framework_equipmentSubtype == 8);
  CHECK(Menu_Framework_enabledStatPages == 7);
  CHECK(fixture.rows[11].unselectable == 1);
  CHECK(fixture.rows[12].unselectable == 1);
  CHECK(fixture.rows[13].unselectable == 1);
  CHECK(fixture.rows[14].state == Menu_Framework_MenuItemState_Inactive);
}

TEST_CASE("F02578 live shield waits for real row idle before accepting exit",
          "[equipment-detail-controllers-live][F02578]") {
  LiveFixture fixture;
  fixture.AddShield();
  fixture.Open(inventory::UpdateLiveShieldDetails);
  fixture.Wait(inventory::UpdateLiveShieldDetails);
  Input_buttonsPressed.allButtons = VS::Input::Cross;
  CHECK(inventory::UpdateLiveShieldDetails(0) == 0);
  CHECK(Menu_Inventory_selectedItemCategoryIconOnTop == 1);
  fixture.IdleRows();
  CHECK(inventory::UpdateLiveShieldDetails(0) == 0);
  CHECK(inventory::UpdateLiveShieldDetails(0) == 0);
  CHECK(Menu_Inventory_selectedItemCategoryIconOnTop == 2);
  CHECK(Menu_Inventory_exitToBattle == 0);
}
