#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Battle/FloatingLabelPlacementLive.hpp"
#include "Battle/FloatingLabelProducersLive.hpp"
#include "Battle/FloatingLabelQueueLive.hpp"
#include "TestSupport/FloatingLabelQueueLiveFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace f = VS::test_support::live_floating_labels;
}  // namespace

TEST_CASE("F00712 production reset clears only its scalar commands",
          "[floating-label-queue-live][F00712]") {
  f::State state;
  VS::Battle::floatingLabelCount = 31;
  VS::Battle::actorMarkersHidden = -123;
  VS::Battle::abilityTimingCueFrames = 300;
  VS::Battle::statusLabelStepTimer = 15;
  VS::Battle::ashleyStatusLabelsSuppressed = 1;
  b::ResetLiveFloatingLabels();
  CHECK(VS::Battle::floatingLabelCount == 0);
  CHECK(VS::Battle::actorMarkersHidden == 0);
  CHECK(VS::Battle::abilityTimingCueFrames == 0);
  CHECK(VS::Battle::statusLabelStepTimer == 0);
  CHECK(VS::Battle::ashleyStatusLabelsSuppressed == 0);
}

TEST_CASE("F00712 production reset clears records without a broad BSS reset",
          "[floating-label-queue-live][F00712]") {
  f::State state;
  state.Populate();
  VS::Battle::actorMarkerSides = 0x12345678;
  VS::Battle::placedLabelCount = 9;
  std::ranges::fill(VS::Battle::actorMarkers, char{77});
  VS::Battle::abilityTimingCuePosition.vz = 123;
  b::ResetLiveFloatingLabels();
  CHECK(VS::Battle::actorMarkerSides == 0x12345678);
  CHECK(VS::Battle::placedLabelCount == 9);
  CHECK(VS::Battle::abilityTimingCuePosition.vz == 123);
  CHECK(std::ranges::all_of(VS::Battle::actorMarkers,
                            [](char marker) { return marker == 77; }));
  CHECK(std::ranges::all_of(state.Labels(), [](const auto& label) {
    return f::Words(label) == f::LabelWords{};
  }));
}

TEST_CASE("F00712 reentry resets only the currently rebound label owner",
          "[floating-label-queue-live][F00712]") {
  f::State state;
  state.Populate();
  std::array<VS::Battle::FloatingLabelSlots, 1> replacement{};
  replacement[0][31].value = 912;
  VS::Battle::floatingLabels = replacement.data();
  b::ResetLiveFloatingLabels();
  CHECK(replacement[0][31].value == 0);
  CHECK(state.labels[0][31].value == 531);
  replacement[0][5].frame = 17;
  b::ResetLiveFloatingLabels();
  CHECK(replacement[0][5].frame == 0);
  CHECK(state.labels[0][5].frame == 17);
}

TEST_CASE("F00713 production producer preserves non-world-owned fields",
          "[floating-label-queue-live][F00713]") {
  f::State state;
  state.Populate();
  auto expected = state.labels[0][0];
  expected.position.vx = -32768;
  expected.position.vy = 32767;
  expected.position.vz = -1;
  expected.key.bytes[0] = 3;
  expected.frame = 0;
  const std::array<std::uint16_t, 3> position{0x8000, 0x7FFF, 0xFFFF};
  CHECK(b::QueueLiveWorldLockedLabel(position) == 0);
  CHECK(f::Words(state.labels[0][0]) == f::Words(expected));
  CHECK(VS::Battle::floatingLabelCount == 1);
}

TEST_CASE("F00721 production advancement accepts a standalone borrowed label",
          "[floating-label-queue-live][F00721]") {
  f::State state;
  VS::Battle::floatingLabels = nullptr;
  VS::Battle::floatingLabelCount = -123;
  VS::Battle::FloatingLabel label{};
  label.position.pad = -32768;
  label.frame = 255;
  label.value = 123;
  b::AdvanceLiveFloatingLabelQueue(&label);
  CHECK(label.position.pad == 32767);
  CHECK(label.frame == 0);
  CHECK(label.value == 123);
  CHECK(VS::Battle::floatingLabelCount == -123);
}

TEST_CASE("F00734 full production reservation keeps complete record order",
          "[floating-label-queue-live][F00734]") {
  f::State state;
  state.Populate();
  VS::Battle::floatingLabelCount = 32;
  const auto second = state.labels[0][1];
  const auto last = state.labels[0][31];
  const auto* const reserved = b::ReserveLiveFloatingLabel();
  CHECK(reserved == &state.labels[0][31]);
  CHECK(f::Words(state.labels[0][0]) == f::Words(second));
  CHECK(f::Words(state.labels[0][30]) == f::Words(last));
  CHECK(f::Words(state.labels[0][31]) == f::LabelWords{});
  CHECK(VS::Battle::floatingLabelCount == 32);
}

TEST_CASE("F00735 production inactivity uses only the kind sentinel",
          "[floating-label-queue-live][F00735]") {
  f::State state;
  state.Populate();
  VS::Battle::floatingLabelCount = 4;
  state.labels[0][0].key.bytes[0] = 0;
  state.labels[0][0].key.bytes[1] = 99;
  state.labels[0][1].key.bytes[0] = static_cast<char>(255);
  state.labels[0][2].key.bytes[0] = 0;
  b::CompactLiveFloatingLabels(9);
  CHECK(VS::Battle::floatingLabelCount == 2);
  CHECK(static_cast<unsigned char>(state.labels[0][0].key.bytes[0]) == 255);
  CHECK(state.labels[0][0].key.bytes[1] == 1);
  CHECK(state.labels[0][1].key.bytes[1] == 3);
  CHECK(f::Words(state.labels[0][2]) == f::LabelWords{});
  CHECK(f::Words(state.labels[0][3]) == f::LabelWords{});
}

TEST_CASE("F00736 production removal preserves neighbors outside its live tail",
          "[floating-label-queue-live][F00736]") {
  f::State state;
  state.Populate();
  VS::Battle::floatingLabelCount = 3;
  const auto retained = state.labels[0][2];
  const auto outside = state.labels[0][3];
  b::RemoveLiveFloatingLabel(1);
  CHECK(VS::Battle::floatingLabelCount == 2);
  CHECK(f::Words(state.labels[0][1]) == f::Words(retained));
  CHECK(f::Words(state.labels[0][2]) == f::LabelWords{});
  CHECK(f::Words(state.labels[0][3]) == f::Words(outside));
}

TEST_CASE(
    "Production timing-result replacement uses reserve removal and commit",
    "[floating-label-queue-live][F00734][F00736]") {
  f::State state;
  CHECK(b::LiveQueueAbilityTimingResult(12) == 0);
  CHECK(b::LiveQueueAbilityTimingResult(34) == 0);
  CHECK(VS::Battle::floatingLabelCount == 1);
  CHECK(state.labels[0][0].key.halfword == 4);
  CHECK(state.labels[0][0].value == 34);
}

TEST_CASE("Production zero-chain removal precedes its early return",
          "[floating-label-queue-live][F00734][F00736]") {
  f::State state;
  CHECK(b::LiveQueueChainCount(1001) == 0);
  CHECK(state.labels[0][0].value == 999);
  CHECK(b::LiveQueueChainCount(0) == 0);
  CHECK(VS::Battle::floatingLabelCount == 0);
  CHECK(f::Words(state.labels[0][0]) == f::LabelWords{});
  CHECK(b::LiveQueueChainCount(0) == 0);
  CHECK(VS::Battle::floatingLabelCount == 0);
}

TEST_CASE("F00734 all six real producers publish their reserved full tail",
          "[floating-label-queue-live][F00713][F00734]") {
  for (int producer = 0; producer < 6; ++producer) {
    f::State state;
    f::Producers inputs;
    state.Populate();
    for (auto& label : state.Labels()) label.key.bytes[0] = 7;
    VS::Battle::floatingLabelCount = 32;
    const auto before = f::Snapshot(state);
    state.canary.key.bytes[0] = 7;
    const auto canary = f::Words(state.canary);
    REQUIRE(f::Producers::Queue(producer, 123) == 0);
    const auto after = f::Snapshot(state);
    CHECK(VS::Battle::floatingLabelCount == 32);
    CHECK(std::equal(after.begin(), after.end() - 1, before.begin() + 1));
    CHECK(f::Words(state.canary) == canary);
    CHECK(after.back() != f::LabelWords{});
  }
}

TEST_CASE("F00734 all six real producers preserve ordinary nonfull extent",
          "[floating-label-queue-live][F00713][F00734]") {
  for (int producer = 0; producer < 6; ++producer) {
    f::State state;
    f::Producers inputs;
    state.Populate();
    const auto before = f::Snapshot(state);
    REQUIRE(f::Producers::Queue(producer, 123) == 0);
    const auto after = f::Snapshot(state);
    CHECK(VS::Battle::floatingLabelCount == 1);
    CHECK(std::equal(after.begin() + 1, after.end(), before.begin() + 1));
  }
}

TEST_CASE("F00713 repeated production saturation evicts exactly one oldest",
          "[floating-label-queue-live][F00713][F00734]") {
  f::State state;
  state.Populate();
  VS::Battle::floatingLabelCount = 32;
  for (int value = 0; value < 96; ++value) {
    const auto before = f::Snapshot(state);
    const std::array<std::uint16_t, 3> position{
        static_cast<std::uint16_t>(value), 22, 33};
    REQUIRE(b::QueueLiveWorldLockedLabel(position) == 0);
    const auto after = f::Snapshot(state);
    CHECK(VS::Battle::floatingLabelCount == 32);
    CHECK(std::equal(after.begin(), after.end() - 1, before.begin() + 1));
    CHECK(state.labels[0][31].position.vx == value);
  }
}

TEST_CASE("Full actor projection early returns do not publish or undo reserve",
          "[floating-label-queue-live][F00734]") {
  f::State state;
  f::Producers inputs;
  state.Populate();
  VS::Battle::floatingLabelCount = 32;
  inputs.actor.instance.reactionState = 1;
  const auto before = f::Snapshot(state);
  CHECK(b::LiveQueueActorStatusLabel(1, 251, 3, 128) == 0);
  const auto after = f::Snapshot(state);
  CHECK(VS::Battle::floatingLabelCount == 32);
  CHECK(std::equal(after.begin(), after.end() - 1, before.begin() + 1));
  CHECK(after.back() == f::LabelWords{});
  CHECK(b::LiveQueueActorValueLabel(1, -1, 123, 128) == 0);
  CHECK(VS::Battle::floatingLabelCount == 32);
  CHECK(state.labels[0][31].key.bytes[0] == 0);
}

TEST_CASE("F00734 actual pending consumer touches exactly the full32 prefix",
          "[floating-label-queue-live][F00734]") {
  f::State state;
  state.Populate();
  VS::Battle::floatingLabelCount = 32;
  for (auto& label : state.Labels()) label.key.bytes[0] = 7;
  state.canary.key.bytes[0] = 7;
  const auto canary = f::Words(state.canary);
  b::LiveCommitEquipmentStatChanges();
  CHECK(std::ranges::all_of(state.Labels(), [](const auto& label) {
    return label.key.bytes[0] == 6;
  }));
  CHECK(f::Words(state.canary) == canary);
  CHECK(VS::Battle::floatingLabelCount == 32);
}

TEST_CASE("F00734 actual update consumes all32 waiting labels without a33rd",
          "[floating-label-queue-live][F00734]") {
  f::State state;
  f::Producers inputs;
  state.Populate();
  VS::Battle::floatingLabelCount = 32;
  for (auto& label : state.Labels()) {
    label.key.bytes[0] = 6;
    label.position.pad = 2;
  }
  state.canary.key.bytes[0] = 6;
  state.canary.position.pad = 2;
  VS::Battle::abilityTimingCueFrames = 0;
  VS::Battle::statusLabelStepTimer = 21;
  VS::Field::areRoomUpdatesPaused = 1;
  b::LiveUpdateAndDrawFloatingLabels();
  CHECK(std::ranges::all_of(state.Labels(), [](const auto& label) {
    return label.position.pad == 1;
  }));
  CHECK(state.canary.position.pad == 2);
  CHECK(VS::Battle::statusLabelStepTimer == 0);
  CHECK(VS::Battle::floatingLabelCount == 32);
}
