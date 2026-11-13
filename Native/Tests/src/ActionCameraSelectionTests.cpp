#include <array>
#include <cstdint>
#include <limits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/ActionCameraSelection.hpp"
#include "TestSupport/ActionCameraSelectionFixture.hpp"

namespace {
namespace b = VS::Battle;
using VS::test_support::camera_selection::Event;
using VS::test_support::camera_selection::World;

TEST_CASE("F00179 keeps full actor and limb and the returned signed bone",
          "[camera-selection][F00179]") {
  World world;
  b::ActionPoint output{5, 6, 7, 99};
  CHECK(b::ActionTargetLimbPosition(0x10001, 0x10002, output, world));
  CHECK(world.events ==
        std::vector<Event>{{1, 0x10001, 0x10002, 2}, {2, 0x10001, -255, 0}});
  CHECK(output == b::ActionPoint{-1, -5, 9, 99});
  world.events.clear();
  CHECK(b::ActionTargetLimbPosition(-257, -1, output, world));
  CHECK(world.events == std::vector<Event>{{2, -257, 0, 0}});
}
TEST_CASE("F00179 refuses a missing produced position without inventing zero",
          "[camera-selection][F00179]") {
  World world;
  world.position_present = false;
  b::ActionPoint output{5, 6, 7, 99};
  CHECK_FALSE(b::ActionTargetLimbPosition(7, -1, output, world));
  CHECK(output == b::ActionPoint{5, 6, 7, 99});
}
TEST_CASE(
    "F00180 no-target path queries the cached owner again after callbacks",
    "[camera-selection][F00180]") {
  World world;
  world.source = {0, 0, 0, 0};
  world.target = {3, 4, 0, 0};
  world.on_position = [&] {
    world.queue_owner = 1;
    world.queues[0].primary.actor = 250;
  };
  CHECK(b::ActionTargetDistance(world) == 5);
  CHECK(world.events == std::vector<Event>{{2, 0, 0, 0}, {2, 250, 0, 0}});
}
TEST_CASE("F00180 rereads count and target from its retained queue",
          "[camera-selection][F00180]") {
  World world;
  world.source = {0, 0, 0, 0};
  world.on_position = [&] {
    world.queue_owner = 1;
    world.queues[0].count = 65535;
    world.queues[0].target.position_kind = -1;
    world.queues[0].target.SetPosition({3, 4, 0, 99});
  };
  CHECK(b::ActionTargetDistance(world) == 5);
  CHECK(world.positions == 1);
}
TEST_CASE(
    "F00180 keeps both original approximate roots rather than one 3D root",
    "[camera-selection][F00180]") {
  World world;
  world.source = {-2426, 7487, 1009, -4035};
  world.queues[0].count = 1;
  world.queues[0].target.position_kind = 1;
  world.queues[0].target.SetPosition({-892, 4966, -6156, 4966});
  CHECK(b::ActionTargetDistance(world) == 7731);
}
TEST_CASE(
    "F00180 separates negative original table arguments from native clamp",
    "[camera-selection][F00180]") {
  World world;
  world.source = {-25000, 0, -25000, 0};
  auto& queue = world.queues[0];
  queue.count = 1;
  queue.target.position_kind = 1;
  queue.target.SetPosition({25000, 0, 25000, 0});
  CHECK(b::ActionTargetDistance(world).has_value());
  world.source = {0, 0, 0, 0};
  queue.target.SetPosition({0, -32768, -32768, 0});
  world.positions = 0;
  CHECK_FALSE(b::ActionTargetDistance(world));
  queue.target.SetPosition({-32768, 0, -32768, 0});
  world.positions = 0;
  CHECK_FALSE(b::ActionTargetDistance(world));
  world.queue_present = false;
  CHECK_FALSE(b::ActionTargetDistance(world));
  world.queue_present = true;
  world.position_present = false;
  CHECK_FALSE(b::ActionTargetDistance(world));
}
TEST_CASE("F00181 source-relative odd midpoint preserves aliased focus pad",
          "[camera-selection][F00181]") {
  World world;
  world.queues[0].focus = world.target;
  world.on_position = [&] { world.queue_owner = 1; };
  auto& output = world.queues[0].focus;
  CHECK(b::ActionCameraMidpoint(output, world));
  CHECK(output == b::ActionPoint{-2, -4, 6, 42});
  CHECK(world.positions == 1);
}
TEST_CASE(
    "F00181 actor target uses signed limb and any nonzero world discriminator",
    "[camera-selection][F00181]") {
  World world;
  auto& queue = world.queues[0];
  queue.count = 65535;
  queue.target.actor = 255;
  queue.target.limb = 128;
  b::ActionPoint output{0, 0, 0, -99};
  CHECK(b::ActionCameraMidpoint(output, world));
  CHECK(world.events[1] == Event{1, 255, -128, 2});
  CHECK(output == b::ActionPoint{-2, -4, 6, -99});
  queue.target.position_kind = std::numeric_limits<std::int32_t>::min();
  queue.target.SetPosition({32767, -32768, 32767, 55});
  world.positions = 0;
  CHECK(b::ActionCameraMidpoint(output, world));
  CHECK(world.tags == 1);
  CHECK(output == b::ActionPoint{16383, -16386, 16388, -99});
}
TEST_CASE("F00181 refuses missing used queue or skeleton output",
          "[camera-selection][F00181]") {
  World world;
  b::ActionPoint output{1, 2, 3, 4};
  world.queue_present = false;
  CHECK_FALSE(b::ActionCameraMidpoint(output, world));
  world.queue_present = true;
  world.position_present = false;
  CHECK_FALSE(b::ActionCameraMidpoint(output, world));
  CHECK(output == b::ActionPoint{1, 2, 3, 4});
}
TEST_CASE(
    "F00182 always computes even with execute zero and accepts any nonzero",
    "[camera-selection][F00182]") {
  World world;
  world.queues[0].focus = world.target;
  CHECK(b::StepCameraTowardActionMidpoint(0, world));
  CHECK(world.positions == 1);
  CHECK(world.events.size() == 1);
  world.positions = 0;
  CHECK(b::StepCameraTowardActionMidpoint(-1, world));
  CHECK(world.stepped == b::ActionPoint{-2, -4, 6, 0});
  world.queue_present = false;
  CHECK_FALSE(b::StepCameraTowardActionMidpoint(0, world));
}
TEST_CASE("F00183 enemy delay wraps before comparison and never copies preview",
          "[camera-selection][F00183]") {
  World world;
  auto& state = world.states[0];
  state.selecting = true;
  state.actor = 255;
  state.frames = 30;
  state.selected = 9;
  CHECK(b::UpdateInitialTargetSelection(world));
  CHECK(state.frames == 31);
  CHECK(state.selected == 9);
  CHECK(world.events == std::vector<Event>{{7, 1, 0, 0}});
  state.frames = 255;
  CHECK(b::UpdateInitialTargetSelection(world));
  CHECK(state.frames == 0);
  CHECK(world.commits == 1);
}
TEST_CASE("F00183 player menu owns initial output but copies current owner",
          "[camera-selection][F00183]") {
  World world;
  world.states[0].selecting = true;
  world.states[0].menu = [&] {
    world.states[0].preview = 13;
    world.current = 1;
    world.states[1].preview = 77;
    return -1;
  };
  CHECK(b::UpdateInitialTargetSelection(world));
  CHECK(world.states[0].preview == 13);
  CHECK(world.states[0].selected == 0);
  CHECK(world.states[1].selected == 77);
  CHECK(world.events == std::vector<Event>{{7, 1, 0, 0}});
}
TEST_CASE("F00183 preserves false-menu writes and refuses only used owners",
          "[camera-selection][F00183]") {
  World world;
  auto& state = world.states[0];
  state.selecting = true;
  state.menu = [&] {
    state.selected = 17;
    return 0;
  };
  CHECK(b::UpdateInitialTargetSelection(world));
  CHECK(state.selected == 17);
  CHECK(world.commits == 0);
  state.menu = [&] {
    world.current = -1;
    return 1;
  };
  CHECK_FALSE(b::UpdateInitialTargetSelection(world));
  CHECK_FALSE(b::UpdateInitialTargetSelection(world));
}
TEST_CASE(
    "F00184 enemy timing and nonselection route to the actual area filter",
    "[camera-selection][F00184]") {
  World world;
  auto& state = world.states[0];
  state.selecting = true;
  state.actor = 1;
  state.frames = 255;
  CHECK(b::UpdateActionAreaSelection(world));
  CHECK(world.events.empty());
  state.frames = 31;
  CHECK(b::UpdateActionAreaSelection(world));
  CHECK(state.frames == 32);
  CHECK(world.events == std::vector<Event>{{8, 0, 0, 0}});
  state.selecting = false;
  CHECK(b::UpdateActionAreaSelection(world));
  CHECK(world.events.size() == 2);
}
TEST_CASE(
    "F00184 initial area owner survives menu replacement and false return",
    "[camera-selection][F00184]") {
  World world;
  world.states[0].selecting = true;
  world.states[0].area = [&] {
    world.current = -1;
    return 0;
  };
  CHECK(b::UpdateActionAreaSelection(world));
  CHECK(world.events.empty());
  world.current = 0;
  world.states[0].area = [&] {
    world.current = -1;
    return -1;
  };
  CHECK(b::UpdateActionAreaSelection(world));
  CHECK(world.events == std::vector<Event>{{8, 0, 0, 0}});
  CHECK_FALSE(b::UpdateActionAreaSelection(world));
}
TEST_CASE("F00185 automatic commit clears selected preview lazily",
          "[camera-selection][F00185]") {
  World world;
  auto& state = world.states[0];
  state.selected = 255;
  state.action = 65535;
  world.targeting = std::nullopt;
  CHECK(b::UpdateFinalTargetSelection(world));
  CHECK(state.selected == 0);
  CHECK(world.events == std::vector<Event>{{7, 2, 0, 0}});
  CHECK(world.definition_reads == 0);
}
TEST_CASE("F00185 enemy wait wraps before clearing selected preview",
          "[camera-selection][F00185]") {
  World world;
  auto& state = world.states[0];
  state.selecting = true;
  state.actor = 255;
  state.frames = 255;
  state.selected = 99;
  CHECK(b::UpdateFinalTargetSelection(world));
  CHECK(state.frames == 0);
  CHECK(state.selected == 99);
  CHECK(world.events.empty());
}
TEST_CASE("F00185 ready enemy clears selected preview and commits mode two",
          "[camera-selection][F00185]") {
  World world;
  auto& state = world.states[0];
  state.selecting = true;
  state.actor = 255;
  state.selected = 99;
  state.frames = 30;
  CHECK(b::UpdateFinalTargetSelection(world));
  CHECK(state.selected == 0);
  CHECK(world.events == std::vector<Event>{{7, 2, 0, 0}});
  CHECK(world.definition_reads == 0);
}
TEST_CASE(
    "F00185 cancelled current action with nonzero category flag reinitializes",
    "[camera-selection][F00185]") {
  World world;
  world.states[0].selecting = true;
  world.states[0].menu = [&] {
    world.current = 1;
    world.states[1].preview = 255;
    world.states[1].action = 255;
    return -1;
  };
  world.targeting = true;
  CHECK(b::UpdateFinalTargetSelection(world));
  CHECK(world.states[1].selected == 255);
  CHECK(world.events == std::vector<Event>{{9, 255, 0, 0}, {10, 255, 0, 0}});
  CHECK(world.commits == 0);
}
TEST_CASE(
    "F00185 cancelled action cleanup keeps the five production services "
    "ordered",
    "[camera-selection][F00185]") {
  World world;
  auto& state = world.states[0];
  state.selecting = true;
  state.preview = 255;
  state.menu = [] { return 1; };
  CHECK(b::UpdateFinalTargetSelection(world));
  CHECK(world.events == std::vector<Event>{{9, 0, 0, 0},
                                           {11, 0, 0, 0},
                                           {12, 0, 0, 0},
                                           {13, 0, 0, 0},
                                           {14, 0, 0, 0},
                                           {15, 0, 0, 0}});
  CHECK(world.commits == 0);
  world.targeting = std::nullopt;
  CHECK_FALSE(b::UpdateFinalTargetSelection(world));
}
TEST_CASE(
    "F00185 reloads after selected store and keeps unused definitions lazy",
    "[camera-selection][F00185]") {
  World world;
  auto& state = world.states[0];
  state.selecting = true;
  state.menu = [] { return -1; };
  state.preview = 255;
  state.selected_callback = [&] { world.current = 1; };
  world.states[1].selected = 2;
  world.targeting = std::nullopt;
  CHECK(b::UpdateFinalTargetSelection(world));
  CHECK(world.definition_reads == 0);
  CHECK(world.commits == 1);
  world.current = 0;
  state.menu = [&] {
    state.selected = 77;
    return 0;
  };
  CHECK(b::UpdateFinalTargetSelection(world));
  CHECK(state.selected == 77);
  world.current = -1;
  CHECK_FALSE(b::UpdateFinalTargetSelection(world));
}
TEST_CASE(
    "F00186 inactive modes do not touch action state and other zooms need no "
    "queue",
    "[camera-selection][F00186]") {
  World world;
  world.queue_present = false;
  world.zoom = std::nullopt;
  for (int mode : {0, 2, 4, -1}) {
    world.mode = mode;
    CHECK(b::UpdateActionCameraZoom(world));
  }
  world.mode = 3;
  CHECK_FALSE(b::UpdateActionCameraZoom(world));
  world.zoom = -1;
  CHECK(b::UpdateActionCameraZoom(world));
  CHECK(world.starts == 1);
  CHECK(world.events == std::vector<Event>{{6, 0, 0, 0}});
}
TEST_CASE(
    "F00186 readiness uses current displacement yaw and mode after callbacks",
    "[camera-selection][F00186]") {
  World world;
  world.on_tracking = [&] { world.mode = 2; };
  CHECK(b::UpdateActionCameraZoom(world));
  CHECK(world.starts == 0);
  world.mode = 1;
  world.on_tracking = [&] {
    world.mode = 3;
    world.yaw = 1;
  };
  CHECK(b::UpdateActionCameraZoom(world));
  CHECK(world.starts == 0);
  world.yaw = 0;
  world.on_tracking = [&] { world.displacement.z = -1; };
  CHECK(b::UpdateActionCameraZoom(world));
  CHECK(world.starts == 0);
  world.on_tracking = [&] { world.displacement = {}; };
  CHECK(b::UpdateActionCameraZoom(world));
  CHECK(world.starts == 1);
}
TEST_CASE(
    "F00186 projection branch keeps cached zoom and reads current geometry",
    "[camera-selection][F00186]") {
  World world;
  world.zoom = 1;
  world.projection = 704;
  world.on_step = [&] {
    world.mode = 2;
    world.zoom = 2;
  };
  world.on_projection = [&] { world.distance = 1600; };
  CHECK(b::UpdateActionCameraZoom(world));
  CHECK(world.projection == 768);
  CHECK(world.distance == 1536);
  CHECK(world.starts == 0);
  CHECK(world.events[2] == Event{5, 768, 0, 0});
  CHECK(world.events.back()[0] == 4);
}
TEST_CASE(
    "F00186 signed projection wraps and current setter projection gates "
    "tracking",
    "[camera-selection][F00186]") {
  World world;
  world.zoom = 1;
  world.projection = std::numeric_limits<std::int32_t>::max();
  CHECK(b::UpdateActionCameraZoom(world));
  CHECK(world.projection == std::numeric_limits<std::int32_t>::min() + 63);
  CHECK(world.starts == 0);
  world.projection = 704;
  world.on_projection = [&] { world.projection = 767; };
  CHECK(b::UpdateActionCameraZoom(world));
  CHECK(world.starts == 0);
}
TEST_CASE("F00186 zoom out clamps distance and waits for exact far readiness",
          "[camera-selection][F00186]") {
  World world;
  world.zoom = 2;
  world.distance = 2200;
  CHECK(b::UpdateActionCameraZoom(world));
  CHECK(world.distance == 2304);
  CHECK(world.starts == 1);
  world.distance = 2305;
  CHECK(b::UpdateActionCameraZoom(world));
  CHECK(world.distance == 2305);
  CHECK(world.starts == 1);
  world.queue_present = false;
  CHECK_FALSE(b::UpdateActionCameraZoom(world));
}
}  // namespace
