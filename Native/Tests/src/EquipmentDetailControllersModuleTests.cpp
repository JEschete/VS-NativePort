#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Input/Pad.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"

extern "C" {
#include "Boot/OverlayReset.h"
}

namespace {
namespace inventory = VS::Menu::Inventory;

template <typename Entry>
bool IsBound(std::string_view module, unsigned address, Entry entry) {
  const auto* resolved = VS::Content::ResolveModuleAddress(
      {Content_moduleSymbols, Content_moduleSymbolCount}, module, address);
  return resolved == reinterpret_cast<void*>(entry);
}
}  // namespace

TEST_CASE("F02575 resolves both actual weapon controller entries",
          "[equipment-detail-controllers-module][F02575]") {
  CHECK(IsBound("MENU/MENU3.PRG", 0x80102DEC,
                &inventory::UpdateLiveWeaponDetails));
  CHECK(IsBound("MENU/MENUB.PRG", 0x80102EC4,
                &inventory::UpdateLiveWeaponDetails));
}

TEST_CASE("F02576 resolves both actual blade controller entries",
          "[equipment-detail-controllers-module][F02576]") {
  CHECK(IsBound("MENU/MENU3.PRG", 0x80103034,
                &inventory::UpdateLiveBladeDetails));
  CHECK(IsBound("MENU/MENUB.PRG", 0x8010310C,
                &inventory::UpdateLiveBladeDetails));
}

TEST_CASE("F02577 resolves both actual grip controller entries",
          "[equipment-detail-controllers-module][F02577]") {
  CHECK(
      IsBound("MENU/MENU3.PRG", 0x80103220, &inventory::UpdateLiveGripDetails));
  CHECK(
      IsBound("MENU/MENUB.PRG", 0x801032F8, &inventory::UpdateLiveGripDetails));
}

TEST_CASE("F02578 resolves both actual shield controller entries",
          "[equipment-detail-controllers-module][F02578]") {
  CHECK(IsBound("MENU/MENU3.PRG", 0x80103460,
                &inventory::UpdateLiveShieldDetails));
  CHECK(IsBound("MENU/MENUB.PRG", 0x80103538,
                &inventory::UpdateLiveShieldDetails));
}

TEST_CASE("F02576 MENU3 reload retains zero-initialized controller state",
          "[equipment-detail-controllers-module][F02576]") {
  equipment_detail_tests::LiveFixture fixture;
  fixture.AddParts();
  fixture.Open(inventory::UpdateLiveBladeDetails);
  fixture.Wait(inventory::UpdateLiveBladeDetails);
  Input_buttonsPressed.allButtons = VS::Input::Cross;
  CHECK(inventory::UpdateLiveBladeDetails(0) == 0);
  Boot_ResetMenu3Statics();
  fixture.IdleRows();
  Input_buttonsPressed.allButtons = 0;
  CHECK(inventory::UpdateLiveBladeDetails(0) == 1);
  CHECK(Menu_Framework_enabledStatPages == 3);
}

TEST_CASE(
    "F02576 MENUB reload preserves BSS until an explicit controller entry",
    "[equipment-detail-controllers-module][F02576]") {
  equipment_detail_tests::LiveFixture fixture;
  fixture.AddParts();
  fixture.Open(inventory::UpdateLiveBladeDetails);
  fixture.Wait(inventory::UpdateLiveBladeDetails);
  Input_buttonsPressed.allButtons = VS::Input::Cross;
  CHECK(inventory::UpdateLiveBladeDetails(0) == 0);
  Boot_ResetMenubStatics();
  fixture.IdleRows();
  Input_buttonsPressed.allButtons = 0;
  CHECK(inventory::UpdateLiveBladeDetails(0) == 1);
  CHECK(Menu_Framework_enabledStatPages == 3);
}
