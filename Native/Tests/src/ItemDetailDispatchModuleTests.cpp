#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Input/Pad.hpp"
#include "Menu/ItemDetailDispatchLiveFixture.hpp"

extern "C" {
#include "Boot/OverlayReset.h"
}

namespace {
namespace inv = VS::Menu::Inventory;
using item_detail_tests::DispatchLiveFixture;

template <typename Entry>
bool IsBound(std::string_view module, unsigned address, Entry entry) {
  const auto* resolved = VS::Content::ResolveModuleAddress(
      {Content_moduleSymbols, Content_moduleSymbolCount}, module, address);
  return resolved == reinterpret_cast<const void*>(entry);
}
}  // namespace

TEST_CASE("F02568 binds both actual void two-argument icon entries",
          "[item-detail-dispatch-module][F02568]") {
  CHECK(IsBound("MENU/MENU3.PRG", 0x80102A3C, &inv::DrawLiveItemCategoryIcons));
  CHECK(IsBound("MENU/MENUB.PRG", 0x80102B14, &inv::DrawLiveItemCategoryIcons));
}

TEST_CASE("F02579 binds both actual int one-argument armor entries",
          "[item-detail-dispatch-module][F02579]") {
  CHECK(IsBound("MENU/MENU3.PRG", 0x801036BC, &inv::UpdateLiveArmorDetails));
  CHECK(IsBound("MENU/MENUB.PRG", 0x80103794, &inv::UpdateLiveArmorDetails));
}

TEST_CASE("F02580 binds both actual int one-argument gem entries",
          "[item-detail-dispatch-module][F02580]") {
  CHECK(IsBound("MENU/MENU3.PRG", 0x801038E4, &inv::UpdateLiveGemDetails));
  CHECK(IsBound("MENU/MENUB.PRG", 0x801039BC, &inv::UpdateLiveGemDetails));
}

TEST_CASE(
    "F02581 binds both actual dispatcher entries, not description selectors",
    "[item-detail-dispatch-module][F02581]") {
  CHECK(IsBound("MENU/MENU3.PRG", 0x80103AD0, &inv::UpdateLiveItemDetails));
  CHECK(IsBound("MENU/MENUB.PRG", 0x80103BA8, &inv::UpdateLiveItemDetails));
}

TEST_CASE("F02579 real MENU3 reset retains armor return-stage BSS",
          "[item-detail-dispatch-module][F02579]") {
  DispatchLiveFixture fixture;
  fixture.AddArmor();
  fixture.Open(inv::UpdateLiveArmorDetails);
  fixture.Wait(inv::UpdateLiveArmorDetails);
  Input_buttonsPressed.allButtons = VS::Input::Cross;
  CHECK(inv::UpdateLiveArmorDetails(0) == 0);
  Boot_ResetMenu3Statics();
  fixture.IdleRows();
  Input_buttonsPressed.allButtons = 0;
  CHECK(inv::UpdateLiveArmorDetails(0) == 1);
  CHECK(inv::UpdateLiveArmorDetails((20 << 8) | 2) == 0);
}

TEST_CASE("F02580 real MENUB reset retains gem return-stage until a new entry",
          "[item-detail-dispatch-module][F02580]") {
  DispatchLiveFixture fixture;
  fixture.AddGems();
  fixture.Open(inv::UpdateLiveGemDetails);
  fixture.Wait(inv::UpdateLiveGemDetails);
  Input_buttonsPressed.allButtons = VS::Input::Cross;
  CHECK(inv::UpdateLiveGemDetails(0) == 0);
  Boot_ResetMenubStatics();
  fixture.IdleRows();
  Input_buttonsPressed.allButtons = 0;
  CHECK(inv::UpdateLiveGemDetails(0) == 1);
  CHECK(inv::UpdateLiveGemDetails((20 << 8) | 2) == 0);
}

TEST_CASE("F02581 real MENUB reset does not forget the active gem dispatcher",
          "[item-detail-dispatch-module][F02581]") {
  DispatchLiveFixture fixture;
  fixture.AddGems();
  CHECK(inv::UpdateLiveItemDetails((((20 << 8) | 1) << 4) | 5) == 0);
  fixture.IdleRows();
  CHECK(inv::UpdateLiveItemDetails(0) == 0);
  fixture.Wait(inv::UpdateLiveGemDetails);
  Input_buttonsPressed.allButtons = VS::Input::Cross;
  CHECK(inv::UpdateLiveItemDetails(0) == 0);
  Boot_ResetMenubStatics();
  fixture.IdleRows();
  Input_buttonsPressed.allButtons = 0;
  CHECK(inv::UpdateLiveItemDetails(0) == 1);
}
