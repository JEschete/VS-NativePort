#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "TestSupport/ActionPresentationFixture.hpp"

namespace {
namespace b = VS::Battle;
using b::AbilityField;
using VS::test_support::presentation::World;
std::size_t Field(AbilityField field) {
  return static_cast<std::size_t>(field);
}

TEST_CASE(
    "F00100 retains its queue owner but rereads state after script and pose",
    "[action-presentation][F00100]") {
  auto owner = std::make_unique<World>();
  auto& world = *owner;
  auto& action = world.states.at(0).queue.at(0).data;
  action.action = 22;
  world.states.at(1).index = 0x80000005;
  world.states.at(1).fields.at(Field(AbilityField::kChainAbilitySlot)) = 3;
  world.states.at(1).selection.data.focus = {1, 2, 3, 4};
  world.mappings.at(2) = 255;
  world.frame = 0xFFFFFFFF;
  world.callback = [&](std::uint32_t address) {
    if (address == 0x800BEC14) world.current = 1;
    if (address == 0x8009EC9C) action.primary.actor = 2;
  };
  REQUIRE(b::StartActionAnimation(world));
  CHECK(world.events ==
        std::vector<std::uint32_t>{
            0x800BEC14, 0xB8,       1,    0x8009EC9C, 0,          1,
            2,          3,          4,    233,        0x80000005, 0x8009F8DC,
            2,          0x80045D64, 0x7E, 0x34,       0x80045D64, 0x7E,
            0x35,       0x80045D64, 0x7E, 0x36,       0x800CB50C});
  CHECK(world.Current().fields.at(Field(AbilityField::kAbilityWindow)) ==
        65534);
  CHECK(world.Current().fields.at(Field(AbilityField::kChainAbilitySlot)) == 0);
  CHECK(world.phase == 7);
}
TEST_CASE(
    "F00100 selects pose boundaries and refuses only a used invalid chain slot",
    "[action-presentation][F00100]") {
  auto world = std::make_unique<World>();
  constexpr std::array kActions{140, 141, 183, 184, 223, 224};
  constexpr std::array<std::uint32_t, 6> kPoses{
      0x8009E2E0, 0x8009F298, 0x8009F298, 0x8009EFEC, 0x8009EFEC, 0x8009E2E0};
  for (std::size_t i = 0; i < kActions.size(); ++i) {
    world->states.at(0).queue.at(0).data.action = kActions.at(i);
    world->presentation.at(kActions.at(i)) = true;
    world->events.clear();
    REQUIRE(b::StartActionAnimation(*world));
    CHECK(world->events.front() == kPoses.at(i));
    CHECK(world->speed == 4);
  }
  world->Current().index = 1;
  world->Current().queue.at(1).data.action = 39;
  world->events.clear();
  CHECK_FALSE(b::StartActionAnimation(*world));
  CHECK(world->events == std::vector<std::uint32_t>{0x800BEC14, 0xB8, 1});
  world->events.clear();
  world->Current().queue.at(1).data.action = 40;
  REQUIRE(b::StartActionAnimation(*world));
  CHECK(world->events.at(3) == 0x8009EE9C);
}
TEST_CASE(
    "F00101 publishes phase six before effects and preserves callback writes",
    "[action-presentation][F00101]") {
  auto world = std::make_unique<World>();
  world->callback = [&](std::uint32_t) { world->phase = 91; };
  b::AdvanceActionEffectPhase(*world);
  CHECK(world->events == std::vector<std::uint32_t>{0x800CF920, 6});
  CHECK(world->phase == 91);
}
TEST_CASE(
    "F00108 caches preview after preparation and uses fresh cancellation actor",
    "[action-presentation][F00108]") {
  auto world = std::make_unique<World>();
  world->callback = [&](std::uint32_t address) {
    if (address == 0x8006EF10) {
      world->current = 1;
      world->Current().preview = 255;
      world->Current().selection.data.primary.actor = 1;
    }
    if (address == 0x800C16DC) world->Current().preview = 0;
    if (address == 0x800CB114) {
      world->Current().selection.data.primary.actor = 2;
    }
  };
  REQUIRE(b::CommitSelectedAction(-1, *world));
  CHECK(world->events ==
        std::vector<std::uint32_t>{0x8006EF10, 0x800C16DC, 0x800C06E0,
                                   0x800C0738, 0x800C05B4, 0x800CB114,
                                   0x800E4C64, 2, 0x8006FBCC, 0});
  CHECK(world->Current().selection.Id() == 0);
}
void ChangeCommitObservations(World& world, std::uint32_t address) {
  if (address == 0x80085B10) {
    world.current = 1;
    world.Current().selection.data.action = 184;
  }
  if (address == 0x80077F70) {
    world.Current().selection.data.action = 232;
    world.zooms.at(232) = 1;
  }
  if (address == 0x8009E5C4) {
    world.Current().selection.data.primary.actor = 2;
  }
}
TEST_CASE(
    "F00108 consumes the selected item and keeps postresolution camera and "
    "score reads fresh",
    "[action-presentation][F00108]") {
  auto world = std::make_unique<World>();
  world->selection_kind = 8;
  world->item = 65535;
  world->category = 10;
  world->attacks.at(0) = 65534;
  world->distance = 449;
  world->callback = [&](std::uint32_t address) {
    ChangeCommitObservations(*world, address);
  };
  REQUIRE(b::CommitSelectedAction(0, *world));
  CHECK(world->selection_kind == 0);
  CHECK(world->attacks.at(0) == 65535);
  CHECK(world->heals == 1);
  CHECK(world->Current().zoom == 0);
  CHECK(std::ranges::find(world->events, 0x80070CAC) != world->events.end());
  CHECK(std::ranges::find(world->events, 0x800CCB9C) != world->events.end());
  CHECK(world->phase == 4);
  CHECK(world->events.back() == 0);
}
TEST_CASE(
    "F00108 keeps count-zero first target and saturation rules without "
    "category modulo",
    "[action-presentation][F00108]") {
  auto world = std::make_unique<World>();
  world->Current().selection.data.action = 1;
  world->category = 11;
  CHECK_FALSE(b::CommitSelectedAction(2, *world));
  CHECK(std::ranges::find(world->events, 0x80077F70) == world->events.end());
  world->category = 9;
  world->attacks.at(9) = 65535;
  world->distance = -1;
  world->zooms.at(1) = 1;
  REQUIRE(b::CommitSelectedAction(2, *world));
  CHECK(world->attacks.at(9) == 65535);
  CHECK(world->Current().zoom == 1);
  world->Current().selection.data.action = 77;
  world->heals = 1000;
  REQUIRE(b::CommitSelectedAction(6, *world));
  CHECK(world->heals == 1000);
  CHECK(std::ranges::find(world->events, 0x800708EC) != world->events.end());
}
TEST_CASE(
    "F00109 preserves current postdispatch action and finishes after callbacks",
    "[action-presentation][F00109]") {
  auto world = std::make_unique<World>();
  auto& action = world->Current().queue.at(0).data;
  action.action = 60;
  action.affinity = 255;
  world->presentation.at(60) = true;
  world->statuses = 0x8000;
  world->effects.at(60) = 39;
  action.targets.front().position_kind = 4;
  world->callback = [&](std::uint32_t address) {
    if (address == 0x800CF0E8) {
      action.action = 223;
      world->current = 1;
    }
    if (address == 0x800CB654) {
      world->information = true;
      action.action = 222;
    }
    if (address == 0x800CB2B8) world->phase = 99;
  };
  REQUIRE(b::StartActionPresentation(*world));
  CHECK(world->states.at(0).fields.at(
            Field(AbilityField::kAbilityInputAttempted)) == 1);
  CHECK(world->dispatched.effect == 36);
  CHECK(world->dispatched.count == 0);
  CHECK(world->assets == std::vector<b::PresentationAsset>{{0, 4, 3, 0}});
  CHECK(world->events.at(world->events.size() - 2) == 222);
  CHECK(world->phase == 5);
}
TEST_CASE(
    "F00109 capacity counts compacted unique targets and never dispatches "
    "partial effects",
    "[action-presentation][F00109]") {
  auto world = std::make_unique<World>();
  auto& action = world->Current().queue.at(0).data;
  action.action = 60;
  world->presentation.at(60) = true;
  action.count = 17;
  for (auto& target : action.targets) target.position_kind = 1;
  CHECK_FALSE(b::StartActionPresentation(*world));
  CHECK(std::ranges::find(world->events, 0x800CF0E8) == world->events.end());
  CHECK(world->phase != 5);
  for (auto& target : action.targets) {
    target.position_kind = 0;
    target.actor = 1;
  }
  action.count = 255;
  REQUIRE(b::StartActionPresentation(*world));
  CHECK(world->dispatched.count == 1);
  CHECK(world->dispatched.targets.front().part == 0);
}
}  // namespace
