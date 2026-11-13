#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Input/Pad.hpp"
#include "Menu/ContainerDetailControllersWorld.hpp"

namespace {
namespace detail = container_detail_controllers_tests::detail;
namespace samples = container_detail_controllers_tests::samples;
using container_detail_controllers_tests::Call;
using container_detail_controllers_tests::Controller;
using container_detail_controllers_tests::Event;
using container_detail_controllers_tests::World;
namespace input = VS::Input;
constexpr std::array<Controller, samples::kControllers> kControllers{
    detail::UpdateWeaponDetails, detail::UpdateBladeDetails,
    detail::UpdateGripDetails,   detail::UpdateShieldDetails,
    detail::UpdateArmorDetails,  detail::UpdateGemDetails,
};

void CheckEntry(Controller update, bool countdown, bool clear_unused) {
  World world;
  detail::ControllerState state;
  state.unused_entry_byte = samples::kOldUnused;
  state.stage = detail::ControllerStage::kReturn;
  const detail::ControllerEntry entry{{samples::kRow}, {samples::kWidePage}};
  CHECK(update(state, world, entry) == 0);
  CHECK(state.selected_row == samples::kRow);
  CHECK(state.selected_page == samples::kWidePage);
  CHECK(state.animation_step == (countdown ? samples::kFrames : 0));
  CHECK(state.unused_entry_byte == (clear_unused ? 0 : samples::kOldUnused));
  CHECK(world.calls == std::vector<Call>{{Event::kBegin, {samples::kRow}}});
  CHECK(update(state, world, detail::ControllerEntry{{0}, {0}}) == 0);
  CHECK(state.selected_page == 0);
  CHECK(state.stage == detail::ControllerStage::kInitialize);
}

void CheckExit(Controller update, World& world, detail::ControllerState& state,
               bool grip, detail::ControllerEntry entry) {
  world.calls.clear();
  world.pressed = input::Cross | input::R1;
  CHECK(update(state, world, std::nullopt) == 0);
  CHECK(world.calls.front().event == Event::kExit);
  CHECK(state.stage == detail::ControllerStage::kReturn);
  CHECK(world.calls.back().event == (grip ? Event::kDrawSlots : Event::kExit));
  world.ready = samples::kReadyResult;
  CHECK(update(state, world, std::nullopt) == samples::kReadyResult);
  CHECK(update(state, world, entry) == 0);
}

void CheckLifecycle(Controller update, bool grip) {
  World world;
  detail::ControllerState state;
  const detail::ControllerEntry entry{{samples::kRow}, {0}};
  (void)update(state, world, entry);
  world.ready = 0;
  CHECK(update(state, world, std::nullopt) == 0);
  CHECK(state.stage == detail::ControllerStage::kInitialize);
  world.ready = 1;
  (void)update(state, world, std::nullopt);
  for (int tick = 0; tick < samples::kWaitTicks; ++tick) {
    (void)update(state, world, std::nullopt);
  }
  CHECK(state.stage == detail::ControllerStage::kInput);
  CheckExit(update, world, state, grip, entry);
}

void CheckFormattedPage(const World& world, int component_rows) {
  CHECK(world.calls[1] == Call{Event::kSelect, {samples::kWidePage}});
  CHECK(world.calls[2] == Call{Event::kFormat, {samples::kWideSlot}});
  CHECK(world.calls[3] == Call{Event::kStats, {samples::kWideSlot}});
  CHECK(world.observed_text == std::array<char, 2>{'s', 't'});
  const auto components =
      std::ranges::count(world.calls, Event::kComponent, &Call::event);
  CHECK(components == component_rows);
}

void CheckPaging(Controller update, int component_rows) {
  World world;
  detail::ControllerState state;
  state.stage = detail::ControllerStage::kInput;
  state.selected_row = samples::kRow;
  world.next_page = samples::kWidePage;
  world.on_call = [&](const Call& call) {
    if (call.event == Event::kStats) {
      world.names[0][0] = 's';
      world.descriptions[0][0] = 't';
      world.owner = 1;
    }
  };
  CHECK(update(state, world, std::nullopt) == 0);
  CHECK(state.selected_page == samples::kWidePage);
  CheckFormattedPage(world, component_rows);
  world.calls.clear();
  CHECK(update(state, world, std::nullopt) == 0);
  CHECK(world.calls.front().event == Event::kPoll);
}
}  // namespace

TEST_CASE("F02953 entry preserves container byte state and re-entry ordering",
          "[container-detail-controllers][F02953]") {
  CheckEntry(detail::UpdateWeaponDetails, false, true);
}
TEST_CASE("F02954 entry starts its ten-step countdown",
          "[container-detail-controllers][F02954]") {
  CheckEntry(detail::UpdateBladeDetails, true, false);
}
TEST_CASE("F02955 entry clears the unread byte without rendering",
          "[container-detail-controllers][F02955]") {
  CheckEntry(detail::UpdateGripDetails, false, true);
}
TEST_CASE("F02956 repeated shield entry resets animation before readiness",
          "[container-detail-controllers][F02956]") {
  CheckEntry(detail::UpdateShieldDetails, false, true);
}
TEST_CASE("F02957 armor entry keeps its original countdown semantics",
          "[container-detail-controllers][F02957]") {
  CheckEntry(detail::UpdateArmorDetails, true, false);
}
TEST_CASE("F02958 gem entry retains its original unsigned bytes",
          "[container-detail-controllers][F02958]") {
  CheckEntry(detail::UpdateGemDetails, true, false);
}

TEST_CASE("F02953 complete weapon input return and repeat lifecycle",
          "[container-detail-controllers][F02953]") {
  CheckLifecycle(detail::UpdateWeaponDetails, false);
}
TEST_CASE("F02954 blade waits animates exits and returns fullword readiness",
          "[container-detail-controllers][F02954]") {
  CheckLifecycle(detail::UpdateBladeDetails, false);
}
TEST_CASE("F02955 grip footer also runs on the exit input frame",
          "[container-detail-controllers][F02955]") {
  CheckLifecycle(detail::UpdateGripDetails, true);
}
TEST_CASE("F02956 shield readiness and exit lifecycle",
          "[container-detail-controllers][F02956]") {
  CheckLifecycle(detail::UpdateShieldDetails, false);
}
TEST_CASE("F02957 armor lifecycle keeps its selected armor gate",
          "[container-detail-controllers][F02957]") {
  CheckLifecycle(detail::UpdateArmorDetails, false);
}
TEST_CASE("F02958 gem input priority and repeated lifecycle",
          "[container-detail-controllers][F02958]") {
  CheckLifecycle(detail::UpdateGemDetails, false);
}

TEST_CASE("F02953 weapon paging retains halfword tokens and formatter owners",
          "[container-detail-controllers][F02953]") {
  constexpr int kWeaponRows = 5;
  CheckPaging(detail::UpdateWeaponDetails, kWeaponRows);
}
TEST_CASE("F02954 blade paging preserves original text through stats callback",
          "[container-detail-controllers][F02954]") {
  CheckPaging(detail::UpdateBladeDetails, 0);
}
TEST_CASE("F02955 grip paging reloads the footer owner after callbacks",
          "[container-detail-controllers][F02955]") {
  CheckPaging(detail::UpdateGripDetails, 0);
}
TEST_CASE("F02956 shield paging refreshes exactly three current gem rows",
          "[container-detail-controllers][F02956]") {
  CheckPaging(detail::UpdateShieldDetails, samples::kShieldRows);
}
TEST_CASE("F02957 armor paging does not repeat the initialization DP gate",
          "[container-detail-controllers][F02957]") {
  CheckPaging(detail::UpdateArmorDetails, 0);
}
TEST_CASE("F02958 gem paging preserves its full source token",
          "[container-detail-controllers][F02958]") {
  CheckPaging(detail::UpdateGemDetails, 0);
}

TEST_CASE("F02953 component animation reloads the selected order each tick",
          "[container-detail-controllers][F02953]") {
  World world;
  detail::ControllerState state;
  state.stage = detail::ControllerStage::kAnimate;
  world.order[0][0] = 1;
  world.order[1][0] = samples::kWideSlot;
  CHECK(detail::UpdateWeaponDetails(state, world) == 0);
  CHECK(world.calls.back() == Call{Event::kComponent, {1, 1, 1}});
  world.owner = 1;
  CHECK(detail::UpdateWeaponDetails(state, world) == 0);
  CHECK(world.calls.back() ==
        Call{Event::kComponent, {2, samples::kWideSlot, 1}});
  state.animation_step = samples::kFrames;
  world.ready = 0;
  CHECK(detail::UpdateWeaponDetails(state, world) == 0);
  CHECK(state.stage == detail::ControllerStage::kInput);
}

TEST_CASE("F02956 shield alone waits for ready after all ten animation frames",
          "[container-detail-controllers][F02956]") {
  World world;
  detail::ControllerState state;
  state.stage = detail::ControllerStage::kAnimate;
  state.animation_step = samples::kFrames;
  world.ready = 0;
  CHECK(detail::UpdateShieldDetails(state, world) == 0);
  CHECK(state.stage == detail::ControllerStage::kAnimate);
  CHECK(world.calls == std::vector<Call>{{Event::kReady}});
  world.ready = samples::kReadyResult;
  CHECK(detail::UpdateShieldDetails(state, world) == 0);
  CHECK(state.stage == detail::ControllerStage::kInput);
}

TEST_CASE("F02957 reloads both page and current container after stats",
          "[container-detail-controllers][F02957]") {
  World world;
  detail::ControllerState state;
  world.order[0][0] = 1;
  world.order[1][1] = samples::kWideSlot;
  world.armor_shows_dp = false;
  world.on_call = [&](const Call& call) {
    if (call.event == Event::kStats) {
      world.owner = 1;
      state.selected_page = 1;
    }
  };
  CHECK(detail::UpdateArmorDetails(state, world) == 0);
  CHECK(world.calls[3] == Call{Event::kStats, {1}});
  CHECK(world.calls.back() == Call{Event::kArmorGate, {samples::kWideSlot}});
  CHECK(state.stage == detail::ControllerStage::kAnimate);
}

TEST_CASE("F02957 selected armor gate sees the post-statistics selected page",
          "[container-detail-controllers][F02957][issue366]") {
  World world;
  detail::ControllerState state;
  world.order[0][0] = samples::kLastInventoryArmor;
  world.order[0][1] = samples::kLastContainerArmor;
  world.armor_shows_dp = false;
  world.on_call = [&](const Call& call) {
    if (call.event == Event::kStats) state.selected_page = 1;
    if (call.event == Event::kArmorGate) {
      world.armor_shows_dp = call.args[0] == samples::kLastContainerArmor;
    }
  };
  CHECK(detail::UpdateArmorDetails(state, world) == 0);
  CHECK(world.calls[3] == Call{Event::kStats, {samples::kLastInventoryArmor}});
  CHECK(world.calls[5] ==
        Call{Event::kArmorGate, {samples::kLastContainerArmor}});
  CHECK(world.calls.back().event == Event::kDp);
}

TEST_CASE(
    "F02957 rebound selected armor can suppress a previously enabled gate",
    "[container-detail-controllers][F02957][issue366]") {
  World world;
  detail::ControllerState state;
  world.order[0][0] = samples::kLastInventoryArmor;
  world.order[1][0] = samples::kFirstContainerOnlyArmor;
  world.armor_shows_dp = true;
  world.on_call = [&](const Call& call) {
    if (call.event == Event::kStats) world.owner = 1;
    if (call.event == Event::kArmorGate) {
      world.armor_shows_dp = call.args[0] != samples::kFirstContainerOnlyArmor;
    }
  };
  CHECK(detail::UpdateArmorDetails(state, world) == 0);
  CHECK(world.calls[5] ==
        Call{Event::kArmorGate, {samples::kFirstContainerOnlyArmor}});
  CHECK(world.calls.back().event == Event::kArmorGate);
  CHECK(state.stage == detail::ControllerStage::kAnimate);
}

TEST_CASE("F02958 rereads triangle after the exit callback",
          "[container-detail-controllers][F02958]") {
  World world;
  detail::ControllerState state;
  state.stage = detail::ControllerStage::kInput;
  world.pressed = input::Cross;
  world.on_call = [&](const Call& call) {
    if (call.event == Event::kExit) world.pressed = input::Triangle;
  };
  CHECK(detail::UpdateGemDetails(state, world) == 0);
  CHECK(world.triangle);
  CHECK(world.calls == std::vector<Call>{{Event::kExit, {0}}});
}

TEST_CASE("Container invalid controller stages are lazy no-ops",
          "[container-detail-controllers][F02953][F02954][F02955][F02956]"
          "[F02957][F02958]") {
  World world;
  detail::ControllerState state;
  state.stage = detail::ControllerStage::kUnrecognized;
  for (const auto controller : kControllers) {
    CHECK(controller(state, world, std::nullopt) == 0);
  }
  CHECK(world.calls.empty());
}

TEST_CASE("F02944 draws eight ordered icons with wrapped last category",
          "[container-detail-controllers][F02944]") {
  World world;
  world.category = 0;
  detail::DrawCategoryIcons(world, 0, 0);
  REQUIRE(world.icons.size() == samples::kIconCount);
  CHECK(world.icons.front().x == samples::kSelectedX);
  CHECK(world.icons.back().x == samples::kSelectedX - samples::kIconPitch);
  CHECK(world.icons.back().selected_palette);
  CHECK(world.icons.front().y == samples::kIconY);
  CHECK(world.calls.size() == samples::kIconCount);
}

TEST_CASE("F02944 merge boundary mutates only returned header owner",
          "[container-detail-controllers][F02944]") {
  World world;
  world.headers[0].x = samples::kMergeX;
  world.on_call = [&](const Call& call) {
    if (call.event == Event::kHeader) world.owner = 1;
  };
  detail::DrawCategoryIcons(world, samples::kLastStep, 1);
  CHECK(world.calls.front() == Call{Event::kTabs, {0}});
  CHECK(world.headers[0].row_icon == samples::kSelectedIcon);
  CHECK(world.headers[1].row_icon == 0);
  CHECK(world.icons.size() == samples::kIconCount - 1);
}

TEST_CASE("F02944 overlay before merge and non-one phase preserve draw branch",
          "[container-detail-controllers][F02944]") {
  World world;
  world.headers[0].x = samples::kBeforeMergeX;
  detail::DrawCategoryIcons(world, 0, 1);
  REQUIRE(world.icons.size() == samples::kIconCount);
  CHECK(world.icons.front().overlay);
  CHECK(world.icons.front().y == samples::kIconPitch);
  world.icons.clear();
  world.calls.clear();
  detail::DrawCategoryIcons(world, std::numeric_limits<std::int32_t>::min(),
                            samples::kByteMask);
  CHECK_FALSE(world.icons.front().overlay);
  CHECK(world.icons.front().y == samples::kIconY);
  CHECK(world.calls.size() == samples::kIconCount);
}

TEST_CASE("F02959 entry supports all six categories and repeated re-entry",
          "[container-detail-controllers][F02959]") {
  World world;
  detail::DispatcherState state;
  world.ready = 0;
  for (std::size_t category = 0; category < samples::kControllers; ++category) {
    const detail::DispatchEntry entry{
        static_cast<VS::Items::ItemCategory>(category),
        detail::ControllerEntry{{samples::kRow}, {samples::kByteMask}}};
    CHECK(detail::UpdateItemDetails(state, world, entry) == 0);
    CHECK(state.category == entry.category);
    CHECK(world.headers[0].item_page == 0);
    CHECK(world.view == detail::DetailViewMode::kDetails);
    CHECK_FALSE(world.triangle);
  }
  if (!world.received_entry.has_value()) {
    FAIL("dispatcher did not pass its nonzero controller entry");
    return;
  }
  CHECK(world.received_entry.value().page.index == samples::kByteMask);
}

TEST_CASE(
    "F02959 close result keeps fullword and restores only container panels",
    "[container-detail-controllers][F02959]") {
  World world;
  detail::DispatcherState state;
  world.controller_result = samples::kReadyResult;
  CHECK(detail::UpdateItemDetails(state, world) == samples::kReadyResult);
  CHECK(world.icons_visible == 1);
  CHECK(world.controls_visible == 1);
  CHECK(world.view == detail::DetailViewMode::kList);
  CHECK(world.calls.back() == Call{Event::kCommand, {1}});
  world.calls.clear();
  world.triangle = true;
  CHECK(detail::UpdateItemDetails(state, world) == -2);
  CHECK(world.calls == std::vector<Call>{{Event::kController, {0}}});
}

TEST_CASE("F02959 ready path reloads selected element before its two draws",
          "[container-detail-controllers][F02959]") {
  World world;
  detail::DispatcherState state;
  world.on_call = [&](const Call& call) {
    if (call.event == Event::kReady) world.selected_element = 0;
  };
  CHECK(detail::UpdateItemDetails(state, world) == 0);
  CHECK(world.cursor_bright);
  CHECK(world.calls == std::vector<Call>{{Event::kController, {0}},
                                         {Event::kReady},
                                         {Event::kTabs, {1}},
                                         {Event::kDetails}});
  world.calls.clear();
  world.ready = 0;
  CHECK(detail::UpdateItemDetails(state, world) == 0);
  CHECK(world.calls.size() == 2);
}

TEST_CASE("F02959 refuses invalid category after original entry side effects",
          "[container-detail-controllers][F02959]") {
  World world;
  detail::DispatcherState state;
  const detail::DispatchEntry entry{
      static_cast<VS::Items::ItemCategory>(samples::kControllers),
      std::nullopt};
  CHECK_THROWS_AS(detail::UpdateItemDetails(state, world, entry),
                  std::out_of_range);
  CHECK(world.view == detail::DetailViewMode::kDetails);
  CHECK(world.headers[0].item_page == 0);
  CHECK(world.calls.back() == Call{Event::kCommand, {0}});
}
