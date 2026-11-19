#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Effects/Runtime.h"
#include "Effects/Timeline.hpp"
#include "Effects/TimelineCommands.hpp"
#include "Effects/TimelineTracks.hpp"
#include "Effects/TimelineTracksLive.hpp"
#include "Effects/Transforms.hpp"

namespace {
namespace fx = VS::Effects;
using fx::ColourKey;
using fx::ColourShift;
using fx::TimelineKeyAdvance;
using fx::TimelineKeySetup;

// A key due on frame 12: red down by 128, green unchanged, blue up by 127.
constexpr ColourKey DueKey() {
  return {.frame = 12,
          .duration = -5,
          .red = 0,
          .green = 0x80,
          .blue = 0xFF,
          .mode = 9};
}
constexpr ColourShift DueShift() {
  return {.mode = 9, .duration = -5, .red = -128, .green = 0, .blue = 127};
}
constexpr ColourKey kKey = DueKey();
constexpr ColourShift kShift = DueShift();

fx::CommandEndpoint Actor(std::uint8_t actor) {
  return {.type = fx::kActorPartEndpoint, .actor = actor, .point = {}};
}

fx::CommandEndpoint RoomPoint(std::uint8_t actor) {
  return {.type = fx::kRoomPointEndpoint, .actor = actor, .point = {}};
}

// Records the visibility queries, the shifts and the track advances.
class RecordingColourHost final : public fx::ColourTrackHost {
 public:
  RecordingColourHost(std::vector<std::uint8_t> visible, bool more_keys)
      : visible_(std::move(visible)), more_keys_(more_keys) {}

  bool ActorVisible(std::uint8_t actor) override {
    queried.push_back(actor);
    return std::ranges::find(visible_, actor) != visible_.end();
  }
  void ShiftActorColours(std::uint8_t actor,
                         const ColourShift& shift) override {
    shifted.push_back(actor);
    shifts.push_back(shift);
  }
  void ShiftSceneColours(const ColourShift& shift) override {
    scene.push_back(shift);
  }
  void ShiftBackgroundColours(const ColourShift& shift) override {
    background.push_back(shift);
  }
  bool AdvanceKey() override {
    ++advances;
    return more_keys_;
  }

  std::vector<std::uint8_t> queried;
  std::vector<std::uint8_t> shifted;
  std::vector<ColourShift> shifts;
  std::vector<ColourShift> scene;
  std::vector<ColourShift> background;
  int advances = 0;

 private:
  std::vector<std::uint8_t> visible_;
  bool more_keys_;
};

// A track of `kind` over `keys`, before its first key.
fx::LiveTimelineTrack TrackOver(std::uint8_t kind,
                                std::span<const std::uint32_t> keys,
                                std::uint8_t key_count) {
  fx::LiveTimelineTrack track{};
  track.keys = keys.data();
  track.header.kind = kind;
  track.header.key_count = key_count;
  track.key_index = -1;
  return track;
}

// Two colour keys: the first due on frame 3, the second on frame 7.
std::array<std::uint32_t, 4> ColourKeys() {
  constexpr std::uint32_t kFirst = 3 | (10U << 16);
  constexpr std::uint32_t kSecond = 7 | (20U << 16);
  constexpr std::uint32_t kChannels = 0x01808080;
  return {kFirst, kChannels, kSecond, kChannels};
}
}  // namespace

TEST_CASE("Track keys advance to the last and name their kind's setup",
          "[timeline-tracks]") {
  CHECK(fx::AdvanceTimelineKey(-1, 3, fx::kParticleEmissionTrack) ==
        TimelineKeyAdvance{.key_index = 0,
                           .setup = TimelineKeySetup::kParticleEmission});
  CHECK(fx::AdvanceTimelineKey(1, 3, fx::kCameraShakeTrack) ==
        TimelineKeyAdvance{.key_index = 2,
                           .setup = TimelineKeySetup::kCameraShake});
  CHECK(fx::AdvanceTimelineKey(0, 2, 12) ==
        TimelineKeyAdvance{.key_index = 1, .setup = TimelineKeySetup::kNone});
  CHECK_FALSE(fx::AdvanceTimelineKey(2, 3, 1).has_value());
  CHECK_FALSE(fx::AdvanceTimelineKey(-1, 0, 1).has_value());
  // The last halfword index never wraps to the first key.
  CHECK_FALSE(fx::AdvanceTimelineKey(0x7FFF, 0xFF, 4).has_value());
}

TEST_CASE("Particle and shake keys take their timing from their own bits",
          "[timeline-tracks]") {
  CHECK(fx::ParticleKeyTimingOf(0xFFFFFFFF) ==
        fx::ParticleKeyTiming{.timer = 0xFF, .parameter = 0x3F});
  constexpr std::uint32_t kParticle = (0x15U << 20) | (0x3CU << 8) | 0x77U;
  CHECK(fx::ParticleKeyTimingOf(kParticle) ==
        fx::ParticleKeyTiming{.timer = 0x3C, .parameter = 0x15});

  std::vector<std::uint8_t> curves;
  const fx::CurveDuration duration = [&curves](std::uint8_t curve) {
    curves.push_back(curve);
    return curve == 7 ? 0x12345 : 0x18000;
  };
  CHECK(fx::ShakeKeyTimer((7U << 4) | 0xFU | (1U << 9), duration) == 0x2345);
  CHECK(fx::ShakeKeyTimer(0xFFFFFFFF, duration) == -0x8000);
  CHECK(curves == std::vector<std::uint8_t>{7, 0x1F});
}

TEST_CASE("Colour tracks wait for their key's frame", "[timeline-tracks]") {
  CHECK(fx::ColourShiftOf(kKey) == kShift);

  RecordingColourHost waiting({}, true);
  CHECK(fx::TickSceneColourTrack(kKey, 11, waiting));
  CHECK(fx::TickBackgroundColourTrack(kKey, -12, waiting));
  CHECK(waiting.scene.empty());
  CHECK(waiting.background.empty());
  CHECK(waiting.advances == 0);
}

TEST_CASE("Due scene and background colour keys shift and advance",
          "[timeline-tracks]") {
  RecordingColourHost due({}, false);
  CHECK_FALSE(fx::TickSceneColourTrack(kKey, 12, due));
  CHECK_FALSE(fx::TickBackgroundColourTrack(kKey, 12, due));
  CHECK(due.scene == std::vector<ColourShift>{kShift});
  CHECK(due.background == std::vector<ColourShift>{kShift});
  CHECK(due.advances == 2);
}

TEST_CASE("Actor colour tracks shift only visible actor endpoints",
          "[timeline-tracks]") {
  RecordingColourHost host({4}, true);
  CHECK(fx::TickSourceActorColourTrack(kKey, 12, Actor(4), host));
  CHECK(fx::TickSourceActorColourTrack(kKey, 12, Actor(5), host));
  // A room point is never queried, even when its owner is visible.
  CHECK(fx::TickSourceActorColourTrack(kKey, 12, RoomPoint(4), host));
  CHECK(fx::TickSourceActorColourTrack(kKey, 11, Actor(4), host));
  CHECK(host.queried == std::vector<std::uint8_t>{4, 5});
  CHECK(host.shifted == std::vector<std::uint8_t>{4});
  CHECK(host.shifts == std::vector<ColourShift>{kShift});
  CHECK(host.advances == 3);
}

TEST_CASE("Target colour track shifts every action target or the receiver",
          "[timeline-tracks]") {
  const std::array targets{Actor(1), RoomPoint(2), Actor(3), Actor(4)};
  RecordingColourHost all({1, 2, 4, 9}, true);
  CHECK(fx::TickTargetActorColourTrack(kKey, 12, targets, Actor(9), true, all));
  CHECK(all.queried == std::vector<std::uint8_t>{1, 3, 4});
  CHECK(all.shifted == std::vector<std::uint8_t>{1, 4});

  RecordingColourHost receiver({1, 9}, false);
  CHECK_FALSE(fx::TickTargetActorColourTrack(kKey, 12, targets, Actor(9), false,
                                             receiver));
  CHECK(receiver.shifted == std::vector<std::uint8_t>{9});

  // With no targets the key still ends.
  RecordingColourHost none({}, true);
  CHECK(fx::TickTargetActorColourTrack(kKey, 12, {}, Actor(9), true, none));
  CHECK(none.queried.empty());
  CHECK(none.advances == 1);
}

TEST_CASE("Other actor colour track shifts the visible other actors",
          "[timeline-tracks]") {
  const std::array<std::uint8_t, 3> others{2, 6, 8};
  RecordingColourHost host({6, 8}, true);
  CHECK(fx::TickOtherActorColourTrack(kKey, 12, others, host));
  CHECK(host.queried == std::vector<std::uint8_t>{2, 6, 8});
  CHECK(host.shifted == std::vector<std::uint8_t>{6, 8});

  RecordingColourHost waiting({6}, true);
  CHECK(fx::TickOtherActorColourTrack(kKey, 0, others, waiting));
  CHECK(waiting.queried.empty());
}

TEST_CASE("Live key advance sets up each new particle key",
          "[timeline-tracks]") {
  constexpr std::uint32_t kParticle = (0x15U << 20) | (0x3CU << 8);
  std::array<std::uint32_t, 2> keys{kParticle, 0x3FU << 20};
  fx::LiveTimelineTrack track =
      TrackOver(fx::kParticleEmissionTrack, keys, keys.size());
  CHECK(fx::LiveAdvanceEffectTimelineKey(&track) == 1);
  CHECK(track.key_index == 0);
  CHECK(track.key_timer == 0x3C);
  CHECK(track.key_parameter == 0x15);
  CHECK(fx::LiveAdvanceEffectTimelineKey(&track) == 1);
  CHECK(track.key_timer == 0);
  CHECK(track.key_parameter == 0x3F);
}

TEST_CASE("Live key advance stops on the last key", "[timeline-tracks]") {
  std::array<std::uint32_t, 2> keys{0xFFFFFFFF, 0xFFFFFFFF};
  fx::LiveTimelineTrack track =
      TrackOver(fx::kParticleEmissionTrack, keys, keys.size());
  track.key_index = 1;
  track.key_timer = 4;
  CHECK(fx::LiveAdvanceEffectTimelineKey(&track) == 0);
  CHECK(track.key_index == 1);
  CHECK(track.key_timer == 4);
  CHECK(fx::LiveAdvanceEffectTimelineKey(nullptr) == 0);
}

TEST_CASE("Live key setup refuses a missing or out-of-track key",
          "[timeline-tracks]") {
  std::array<std::uint32_t, 2> keys{0xFFFFFFFF, 0xFFFFFFFF};
  fx::LiveTimelineTrack track = TrackOver(fx::kParticleEmissionTrack, keys, 1);
  track.key_timer = 4;
  track.key_parameter = 9;
  fx::LiveInitializeParticleTimelineKey(&track);
  fx::LiveInitializeShakeTimelineKey(&track);
  track.key_index = 1;
  fx::LiveInitializeParticleTimelineKey(&track);
  track.keys = nullptr;
  track.key_index = 0;
  fx::LiveInitializeParticleTimelineKey(&track);
  fx::LiveInitializeParticleTimelineKey(nullptr);
  fx::LiveInitializeShakeTimelineKey(nullptr);
  CHECK(track.key_timer == 4);
  CHECK(track.key_parameter == 9);
}

TEST_CASE("Live colour tracks read their current key on the instance",
          "[timeline-tracks]") {
  static fx::LiveEffectTimeline timeline;
  static Effects_EffectInstance instance{};
  instance.context.emit_from.type = fx::kRoomPointEndpoint;
  instance.context.timeline = &timeline;
  auto keys = ColourKeys();
  fx::LiveTimelineTrack track = TrackOver(6, keys, 2);
  track.key_index = 0;

  timeline.frame = 7;
  CHECK(fx::LiveTickSourceActorColorTrack(&instance, &track) == 1);
  CHECK(track.key_index == 0);
  timeline.frame = 3;
  CHECK(fx::LiveTickSourceActorColorTrack(&instance, &track) == 1);
  CHECK(track.key_index == 1);
  timeline.frame = 7;
  CHECK(fx::LiveTickSourceActorColorTrack(&instance, &track) == 0);
  CHECK(track.key_index == 1);
}

TEST_CASE("Live colour tracks refuse missing records and extra targets",
          "[timeline-tracks]") {
  static fx::LiveEffectTimeline timeline;
  static Effects_EffectInstance instance{};
  instance.context.emit_to.type = fx::kRoomPointEndpoint;
  auto keys = ColourKeys();
  fx::LiveTimelineTrack track = TrackOver(7, keys, 2);
  track.key_index = 0;

  CHECK(fx::LiveTickSceneColorTrack(&instance, &track) == 0);
  instance.context.timeline = &timeline;
  CHECK(fx::LiveTickBackgroundColorTrack(&instance, nullptr) == 0);
  CHECK(fx::LiveTickOtherActorColorTrack(nullptr, &track) == 0);
  track.key_index = 2;
  CHECK(fx::LiveTickSceneColorTrack(&instance, &track) == 0);

  // Targets past the request's sixteen are not read.
  const VS::Effects::LiveActionEffectRequest saved = Effects_actionRequest;
  Effects_actionRequest = {};
  for (VS::Effects::LiveActionEffectTarget& target :
       Effects_actionRequest.targets) {
    target.type = fx::kRoomPointEndpoint;
  }
  Effects_actionRequest.count = 0xFF;
  track.key_index = 0;
  timeline.frame = 3;
  CHECK(fx::LiveTickTargetActorColorTrack(&instance, &track, 2) == 1);
  CHECK(track.key_index == 1);
  Effects_actionRequest = saved;
}
