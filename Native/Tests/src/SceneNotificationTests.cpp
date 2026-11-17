#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Menu/SceneNotificationWorld.hpp"

namespace {
namespace menu = VS::Menu;
using VS::tests::notifications::Call;
using VS::tests::notifications::World;

void CheckShownBytes(const World& world, int kind) {
  if (kind == 2) {
    CHECK(world.storage[0] == 9);
    CHECK(world.storage[1] == VS::Render::kTextTerminator);
  } else {
    constexpr std::array<std::uint8_t, 4> kPrefix{0xF8, 0, 0xFB, 4};
    CHECK(std::ranges::equal(std::span(world.storage).first(4), kPrefix));
    CHECK(world.storage[4] == (kind == 11 ? 0x25 : 0x33));
  }
}
}  // namespace

TEST_CASE(
    "F01379 publishes allocation and timer before formatted box callbacks",
    "[scene-notification][F01379]") {
  World world;
  const auto kind = GENERATE(2, 11, 12);
  const auto subject = kind == 2 ? 458 : 3;
  REQUIRE(menu::ShowSceneNotification(world.state, world, kind, subject, 1) ==
          menu::SceneNotificationResult::kShown);
  CHECK(world.observed_buffer == world.storage.data());
  CHECK(world.observed_timer == 120);
  CHECK(world.observed_timed == 1);
  CHECK(world.shown_buffer == world.storage.data());
  std::vector expected{Call::kAllocate, Call::kInit, Call::kBind};
  if (kind == 11) expected.push_back(Call::kShape);
  expected.insert(expected.end(), {Call::kFormat, Call::kSet});
  CHECK(world.calls == expected);
  CheckShownBytes(world, kind);
}

TEST_CASE(
    "F01379 narrows the timed argument before deciding to restart its timer",
    "[scene-notification][F01379]") {
  World world;
  world.state.timer = 77;
  REQUIRE(menu::ShowSceneNotification(world.state, world, 0, 0, 256) ==
          menu::SceneNotificationResult::kShown);
  CHECK(world.state.timed == 0);
  CHECK(world.observed_timed == 0);
  CHECK(world.state.timer == 77);
  CHECK(world.observed_timer == 77);
}

TEST_CASE("F01379 refuses busy or invalid messages before allocating",
          "[scene-notification][F01379]") {
  World world;
  const int failure = GENERATE(0, 1, 2, 3);
  if (failure == 0) world.state.buffer = world.storage;
  if (failure == 1) world.format_allowed = false;
  const auto kind = failure == 2 ? -1 : 0;
  const auto subject = failure == 3 ? 100 : 0;
  const auto expected = failure == 0
                            ? menu::SceneNotificationResult::kAlreadyActive
                            : menu::SceneNotificationResult::kInvalidText;
  CHECK(menu::ShowSceneNotification(world.state, world, kind, subject, 1) ==
        expected);
  CHECK(world.calls.empty());
  CHECK(world.state.timer == 0);
}

TEST_CASE("F01379 releases a short allocation without initializing the box",
          "[scene-notification][F01379]") {
  World world;
  world.allocation_bytes = GENERATE(0U, 111U);
  CHECK(menu::ShowSceneNotification(world.state, world, 0, 0, 1) ==
        menu::SceneNotificationResult::kAllocationFailed);
  std::vector expected{Call::kAllocate};
  if (world.allocation_bytes != 0) expected.push_back(Call::kFree);
  CHECK(world.calls == expected);
  CHECK(world.state.buffer.empty());
  CHECK(world.state.timer == 0);
}

TEST_CASE("F01379 bounds complete formatted text including its optional prefix",
          "[scene-notification][F01379]") {
  World world;
  const bool prefixed = GENERATE(false, true);
  const bool overflow = GENERATE(false, true);
  std::array<std::uint8_t, menu::kSceneNotificationBytes + 1> name{};
  name.fill(1);
  const std::size_t length = (prefixed ? 107U : 112U) + (overflow ? 1U : 0U);
  name[length - 1] = VS::Render::kTextTerminator;
  world.action_name_override = std::span(name).first(length);
  const auto expected = overflow ? menu::SceneNotificationResult::kInvalidText
                                 : menu::SceneNotificationResult::kShown;
  CHECK(menu::ShowSceneNotification(world.state, world, prefixed ? 11 : 0, 0,
                                    0) == expected);
  CHECK(world.calls.empty() == overflow);
  CHECK(world.state.buffer.empty() == overflow);
  if (!overflow) {
    CHECK(world.storage.back() == VS::Render::kTextTerminator);
  }
}

TEST_CASE("F01379 unwinds a refused formatter without publishing dangling text",
          "[scene-notification][F01379]") {
  World world;
  world.format_succeeds = false;
  CHECK(menu::ShowSceneNotification(world.state, world, 0, 0, 1) ==
        menu::SceneNotificationResult::kFormattingFailed);
  const std::vector expected{Call::kAllocate, Call::kInit,    Call::kBind,
                             Call::kFormat,   Call::kDismiss, Call::kFree};
  CHECK(world.calls == expected);
  CHECK(world.state.buffer.empty());
  CHECK(world.released_buffer == world.storage.data());
  CHECK(world.shown_buffer == nullptr);
}

TEST_CASE("F01380 does not tick or poll until an active box has finished",
          "[scene-notification][F01380]") {
  World world;
  const bool active = GENERATE(false, true);
  if (active) world.state.buffer = world.storage;
  world.state.timer = 120;
  world.state.timed = 1;
  CHECK(menu::IsSceneNotificationPending(world.state, world, 2) ==
        (active ? 1 : 0));
  CHECK(world.state.timer == 120);
  CHECK(world.calls.empty());
}

TEST_CASE("F01380 wraps timer subtraction and suppresses polling at expiry",
          "[scene-notification][F01380]") {
  struct Tick {
    std::uint8_t before;
    std::int32_t speed;
    std::uint8_t after;
  };
  constexpr std::array kCases{
      Tick{3, -9, 0}, Tick{4, 9, 251}, Tick{120, 2, 118}, Tick{255, -1, 0},
      Tick{120, std::numeric_limits<std::int32_t>::min(), 120}};
  const auto tick = GENERATE_COPY(Catch::Generators::from_range(kCases));
  World world;
  world.state.buffer = world.storage;
  world.state.timer = tick.before;
  world.state.timed = 255;
  world.done = true;
  CHECK(menu::IsSceneNotificationPending(world.state, world, tick.speed) ==
        (tick.after == 0 ? 0 : -1));
  CHECK(world.state.timer == tick.after);
  CHECK(world.calls.empty() == (tick.after == 0));
}

TEST_CASE("F01380 untimed results retain timer and use wrapping negation",
          "[scene-notification][F01380]") {
  World world;
  world.state.buffer = world.storage;
  world.state.timer = 33;
  world.done = true;
  world.poll_result = GENERATE(0, 2, std::numeric_limits<std::int32_t>::min());
  const auto expected = world.poll_result == 2 ? -2 : world.poll_result;
  CHECK(menu::IsSceneNotificationPending(world.state, world, 9) == expected);
  CHECK(world.state.timer == 33);
  CHECK(world.calls == std::vector{Call::kPoll});
}

TEST_CASE(
    "F01381 dismisses before freeing and repeated release retains timer bytes",
    "[scene-notification][F01381]") {
  World world;
  world.state.buffer = world.storage;
  world.state.timer = 37;
  world.state.timed = 255;
  menu::ReleaseSceneNotification(world.state, world);
  CHECK(world.calls == std::vector{Call::kDismiss, Call::kFree});
  CHECK(world.released_buffer == world.storage.data());
  CHECK(world.state.buffer.empty());
  CHECK(world.state.timer == 37);
  CHECK(world.state.timed == 255);
  menu::ReleaseSceneNotification(world.state, world);
  CHECK(world.calls.size() == 2);
}
