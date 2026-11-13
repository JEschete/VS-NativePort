#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Menu/Inventory/DetailsPoliciesLive.hpp"

namespace {
namespace inventory = VS::Menu::Inventory;

template <typename Entry>
bool IsBound(std::string_view module, unsigned address, Entry entry) {
  const auto* resolved = VS::Content::ResolveModuleAddress(
      {Content_moduleSymbols, Content_moduleSymbolCount}, module, address);
  return resolved == reinterpret_cast<void*>(entry);
}
}  // namespace

TEST_CASE("F02566 preserves both original visibility entry bindings",
          "[inventory-detail-helpers-module][F02566]") {
  CHECK(IsBound("MENU/MENU3.PRG", 0x8010296C,
                &inventory::SetLivePanelVisibility));
  CHECK(IsBound("MENU/MENUB.PRG", 0x80104F98,
                &inventory::SetLivePanelVisibility));
}

TEST_CASE("F02567 preserves both original paging entry bindings",
          "[inventory-detail-helpers-module][F02567]") {
  CHECK(
      IsBound("MENU/MENU3.PRG", 0x80102988, &inventory::PollLiveDetailsPaging));
  CHECK(
      IsBound("MENU/MENUB.PRG", 0x80102A60, &inventory::PollLiveDetailsPaging));
}

TEST_CASE("F02569 preserves both original transition entry bindings",
          "[inventory-detail-helpers-module][F02569]") {
  CHECK(IsBound("MENU/MENU3.PRG", 0x80102B78,
                &inventory::BeginLiveDetailsTransition));
  CHECK(IsBound("MENU/MENUB.PRG", 0x80102C50,
                &inventory::BeginLiveDetailsTransition));
}

TEST_CASE("F02571 preserves both original layout entry bindings",
          "[inventory-detail-helpers-module][F02571]") {
  CHECK(IsBound("MENU/MENU3.PRG", 0x80102C44,
                &inventory::InitializeLiveDetailsLayout));
  CHECK(IsBound("MENU/MENUB.PRG", 0x80102D1C,
                &inventory::InitializeLiveDetailsLayout));
}

TEST_CASE("F02573 preserves both original selection entry bindings",
          "[inventory-detail-helpers-module][F02573]") {
  CHECK(
      IsBound("MENU/MENU3.PRG", 0x80102D30, &inventory::SelectLiveDetailsItem));
  CHECK(
      IsBound("MENU/MENUB.PRG", 0x80102E08, &inventory::SelectLiveDetailsItem));
}

TEST_CASE("F02574 preserves both original exit entry bindings",
          "[inventory-detail-helpers-module][F02574]") {
  CHECK(IsBound("MENU/MENU3.PRG", 0x80102D7C, &inventory::ExitLiveDetailsView));
  CHECK(IsBound("MENU/MENUB.PRG", 0x80102E54, &inventory::ExitLiveDetailsView));
}
