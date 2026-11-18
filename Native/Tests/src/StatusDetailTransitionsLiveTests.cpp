#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Status/ActorStatus.h"
#include "Menu/Status/DetailTransitionsLive.hpp"
#include "Menu/StatusTransitionsWorld.hpp"

namespace {
namespace status = VS::Menu::Status;
namespace fw = VS::Menu::Framework;
namespace data = vs_test::status_transitions;
using equipment_detail_tests::ArrayRestore;
using equipment_detail_tests::ValueRestore;

struct LiveTransitionFixture : equipment_detail_tests::LiveFixture {
  ArrayRestore<u_short, std::size(Menu_Status_statusStrings)> saved_status{
      Menu_Status_statusStrings};
  ArrayRestore<u_char, std::size(VS::Menu::Framework::weaponHands)> saved_hands{
      VS::Menu::Framework::weaponHands};
  vs_test::status_components::World text;

  LiveTransitionFixture() {
    std::memcpy(help.data(), text.help.data(), text.help.size());
    std::memcpy(names.data(), text.names.data(), text.names.size());
    std::ranges::copy(text.hands, VS::Menu::Framework::weaponHands);
    Battle_stringContext = {};
    std::fill_n(Menu_Status_statusStrings, 48, u_short{74});
    Menu_Status_statusStrings[74] = 0xE701;
  }
};

template <typename Entry>
Entry ModuleEntry(unsigned address, Entry expected) {
  auto* resolved = VS::Content::ResolveModuleAddress(
      VS::Content::ModuleSymbols(), "MENU/MENU4.PRG", address);
  CHECK(resolved == reinterpret_cast<void*>(expected));
  if (resolved == nullptr) {
    throw std::runtime_error("missing status transition module entry");
  }
  return reinterpret_cast<Entry>(resolved);
}
}  // namespace

TEST_CASE("Status navigation module entry reads the current framework map",
          "[status-transitions-live][F02655]") {
  LiveTransitionFixture fixture;
  auto& link = VS::Menu::Framework::statusViewNavigationMap[3][0];
  ValueRestore saved_link{link};
  link = 9;
  VS::Input::buttonRepeat = 0x1000;
  auto entry = ModuleEntry(0x801034BC, &status::NavigateLiveDetailElements);
  CHECK(entry(3, 0x76) == 9);
  VS::Input::buttonRepeat = 0;
  CHECK(entry(9, 0) == 9);
  CHECK(entry(21, 0) == 21);
  CHECK(VS::Menu::Framework::stringBuf == fixture.description.data());
}

TEST_CASE("Status hoist module entry preserves text and flies the name row out",
          "[status-transitions-live][F02669]") {
  LiveTransitionFixture fixture;
  auto& row = fw::LiveMenuRow(13);
  row = {.state = fw::RowState::kIdle,
         .x = 41,
         .text = {1, 0xE7},
         .sub_text = fixture.description.data()};
  fw::LiveMenuRow(4).state = fw::RowState::kIdle;
  ModuleEntry(0x80104AEC, &status::HoistLiveEquipmentRow)(3);
  CHECK(row.state == fw::RowState::kSlideX);
  CHECK(row.target0 == 155);
  CHECK(row.selected == 1);
  CHECK(row.sub_text == nullptr);
  CHECK(row.text[0] == 1);
  CHECK(row.x == 41);
  CHECK(fw::LiveMenuRow(4).target0 == 320);
}

TEST_CASE("Status title module entry creates real category and equipment rows",
          "[status-transitions-live][F02670]") {
  LiveTransitionFixture fixture;
  const int selected = GENERATE(0, 1, 2);
  const std::array icons{24, 27, 28};
  ModuleEntry(0x80104B38, &status::BeginLiveDetailTransition)(selected);
  const auto& title = fw::LiveMenuRow(32);
  CHECK(title.state == fw::RowState::kDismiss);
  CHECK(title.row_icon == icons[static_cast<std::size_t>(selected)]);
  CHECK(title.text[0] == 1);
  CHECK(title.x == -164);
  CHECK(title.target0 == 16);
  CHECK(fw::LiveMenuRow(selected + 10).state == fw::RowState::kSlideY);
  CHECK(fw::LiveMenuRow(selected + 10).target0 == 18);
}

TEST_CASE("Status initialization module entry binds actual stat page ownership",
          "[status-transitions-live][F02671]") {
  LiveTransitionFixture fixture;
  VS::Menu::Framework::enabledStatPages = 0;
  ModuleEntry(0x80104C0C, &status::InitializeLiveDetailPanel)(1, 5);
  CHECK(VS::Menu::Framework::enabledStatPages == 5);
  CHECK(VS::Menu::Framework::itemStatPage == fw::stat_page::kClass);
  CHECK(fw::LiveMenuRow(32).row_icon == 27);
  CHECK(fw::LiveMenuRow(11).target0 == 18);
  CHECK(VS::Menu::Framework::stringBuf == fixture.description.data());
}

TEST_CASE("Status weapon module entry formats real parts and empty sockets",
          "[status-transitions-live][F02672]") {
  LiveTransitionFixture fixture;
  const int component = GENERATE(1, 2, 3, 4, 5);
  const int init = GENERATE(0, 1);
  const auto typed = data::Weapon();
  auto weapon = std::bit_cast<Items_WeaponLoadout>(typed);
  ModuleEntry(0x80104C40, &status::SetLiveWeaponComponentRow)(component,
                                                              &weapon, init);
  const auto& row = fw::LiveMenuRow(component + 20);
  CHECK(row.row_icon == (component == 1 ? 9 : component == 2 ? 14 : 22));
  CHECK(row.material == (component == 1 ? typed.blade.material : 0));
  CHECK(row.unselectable == (component == 4));
  CHECK(row.x == (init != 0 ? 320 : component < 3 ? 162 : 169));
  CHECK(VS::Items::Live::Typed(weapon) == typed);
  CHECK(VS::Menu::Framework::stringBuf == fixture.description.data());
}

TEST_CASE("Status shield module entry masks slide flags and preserves loadouts",
          "[status-transitions-live][F02673]") {
  LiveTransitionFixture fixture;
  const int component = GENERATE(1, 2, 3);
  const int init = GENERATE(2, 3);
  const auto typed = data::Shield();
  auto shield = std::bit_cast<Items_ShieldLoadout>(typed);
  ModuleEntry(0x80104DFC, &status::SetLiveShieldGemRow)(component, &shield,
                                                        init);
  const auto& row = fw::LiveMenuRow(component + 20);
  CHECK(row.row_icon == 22);
  CHECK(row.unselectable == (component == 2));
  CHECK(row.x == ((init & 1) != 0 ? 320 : 169));
  CHECK(row.state ==
        ((init & 1) != 0 ? fw::RowState::kSlideX : fw::RowState::kIdle));
  CHECK(VS::Items::Live::Typed(shield) == typed);
  CHECK(VS::Menu::Framework::stringBuf == fixture.description.data());
}

TEST_CASE("Status exit module entry drives real rows and optional DP PP hiding",
          "[status-transitions-live][F02674]") {
  LiveTransitionFixture fixture;
  const bool clear = GENERATE(false, true);
  fw::LiveMenuRow(3).state = fw::RowState::kIdle;
  auto& title = fw::LiveMenuRow(fw::kActionRowBase);
  title = {.state = fw::RowState::kIdle, .w = 80};
  fw::SetLiveDpPp(100, 200, 40, 100);
  fw::UpdateLiveDpPpBars(fw::dp_pp_bars::kBoth | fw::dp_pp_bars::kStatic);
  fw::UpdateLiveDpPpBars(0);
  REQUIRE_FALSE(fixture.Sprites().empty());
  ModuleEntry(0x80104F2C, &status::ExitLiveEquipmentDetails)(clear ? 1 : 0);
  CHECK(fw::LiveMenuRow(3).target0 == 320);
  CHECK(title.state == fw::RowState::kDismiss);
  CHECK(title.target0 == -80);
  for (int step = 0; step < 5; ++step) {
    fixture.ResetFrame();
    fw::UpdateLiveDpPpBars(0);
  }
  CHECK(fixture.Sprites().empty() == clear);
  CHECK(VS::Menu::Framework::stringBuf == fixture.description.data());
}

TEST_CASE("Status Live transitions refuse absent workspace without text writes",
          "[status-transitions-live]") {
  LiveTransitionFixture fixture;
  auto weapon = std::bit_cast<Items_WeaponLoadout>(data::Weapon());
  auto shield = std::bit_cast<Items_ShieldLoadout>(data::Shield());
  fixture.description.fill('Q');
  const auto before = fixture.description;
  VS::Menu::Framework::menuItems = nullptr;
  status::HoistLiveEquipmentRow(0);
  status::BeginLiveDetailTransition(0);
  status::InitializeLiveDetailPanel(0, 7);
  status::SetLiveWeaponComponentRow(1, &weapon, 1);
  status::SetLiveShieldGemRow(1, &shield, 1);
  status::ExitLiveEquipmentDetails(1);
  CHECK(fixture.description == before);
  CHECK(VS::Menu::Framework::menuItems == nullptr);
}
