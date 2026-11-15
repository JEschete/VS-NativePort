#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Effects/TimelineCommands.hpp"
#include "Effects/TimelineCommandsLive.hpp"
extern "C" {
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"
}

namespace {
namespace fx = VS::Effects;
using fx::CommandEndpoint;
using fx::CommandEndpoints;
using fx::EndpointPoint;

constexpr EndpointPoint kPoint{.x = -320, .y = 64, .z = 1280, .pad = 7};
constexpr EndpointPoint kOtherPoint{.x = 16, .y = -8, .z = 96, .pad = -3};

CommandEndpoint ActorEndpoint(std::uint8_t actor) {
  return {.type = fx::kActorPartEndpoint, .actor = actor, .point = kOtherPoint};
}

CommandEndpoint PointEndpoint(std::uint16_t type, EndpointPoint point) {
  return {.type = type, .actor = 0, .point = point};
}

// Replays a fixed draw sequence and counts the draws taken.
class Draws {
 public:
  explicit Draws(std::vector<std::int32_t> values)
      : values_(std::move(values)) {}

  fx::RandomDraw Draw() {
    return [this] { return values_.at(taken_++); };
  }
  [[nodiscard]] std::size_t Taken() const { return taken_; }

 private:
  std::vector<std::int32_t> values_;
  std::size_t taken_ = 0;
};
}  // namespace

TEST_CASE("Action impacts name the actor or the point's owner",
          "[timeline-commands]") {
  const CommandEndpoints actors{.source = ActorEndpoint(3),
                                .target = ActorEndpoint(9)};
  CHECK(fx::TargetActionImpact(actors) ==
        fx::ActionImpact{.actor = 9, .point = std::nullopt});
  CHECK(fx::SourceActionImpact(actors) ==
        fx::ActionImpact{.actor = 3, .point = std::nullopt});

  const CommandEndpoints points{.source = PointEndpoint(0, kOtherPoint),
                                .target = PointEndpoint(5, kPoint)};
  CHECK(fx::TargetActionImpact(points) ==
        fx::ActionImpact{.actor = kPoint.pad, .point = kPoint});
  CHECK(fx::SourceActionImpact(points) ==
        fx::ActionImpact{.actor = kOtherPoint.pad, .point = kOtherPoint});
}

TEST_CASE("Room tile and gimmick commands take the target's room point",
          "[timeline-commands]") {
  const CommandEndpoints room{.source = PointEndpoint(5, kOtherPoint),
                              .target = PointEndpoint(5, kPoint)};
  CHECK(fx::TargetRoomTile(room) == kPoint);
  CHECK(fx::TargetGimmick(room) == kPoint);

  const CommandEndpoints actor_target{.source = PointEndpoint(5, kPoint),
                                      .target = ActorEndpoint(2)};
  CHECK_FALSE(fx::TargetRoomTile(actor_target).has_value());
  CHECK_FALSE(fx::TargetGimmick(actor_target).has_value());
  CHECK_FALSE(fx::TargetRoomTile({.target = PointEndpoint(6, kPoint)}));
}

TEST_CASE("Signal and defense timing commands read their own endpoint",
          "[timeline-commands]") {
  const CommandEndpoints endpoints{.source = ActorEndpoint(4),
                                   .emit_from = ActorEndpoint(5),
                                   .target = ActorEndpoint(6),
                                   .emit_to = ActorEndpoint(8)};
  CHECK(fx::TimelineSourceActorToSignal(endpoints) == 4);
  CHECK(fx::DefenseTimingTarget(endpoints) == 8);

  const CommandEndpoints rooms{.source = PointEndpoint(5, kPoint),
                               .emit_from = ActorEndpoint(5),
                               .target = ActorEndpoint(6),
                               .emit_to = PointEndpoint(5, kPoint)};
  CHECK_FALSE(fx::TimelineSourceActorToSignal(rooms).has_value());
  CHECK_FALSE(fx::DefenseTimingTarget(rooms).has_value());
}

TEST_CASE("Weapon brightness of zero clears the emitter's override",
          "[timeline-commands]") {
  const CommandEndpoints endpoints{.source = ActorEndpoint(1),
                                   .emit_from = ActorEndpoint(11)};
  CHECK(fx::SourceWeaponBrightness(endpoints, 0x60) ==
        fx::WeaponBrightness{.actor = 11, .enabled = true, .brightness = 0x60});
  CHECK(fx::SourceWeaponBrightness(endpoints, 0) ==
        fx::WeaponBrightness{.actor = 11, .enabled = false, .brightness = 0});
  CHECK_FALSE(
      fx::SourceWeaponBrightness({.emit_from = PointEndpoint(5, kPoint)}, 9)
          .has_value());
}

TEST_CASE("Fixed reaction variants play without drawing",
          "[timeline-commands]") {
  const CommandEndpoints endpoints{.emit_from = ActorEndpoint(12)};
  Draws draws({});
  CHECK(fx::SourceReactionSound(endpoints, 3, 1, draws.Draw()) ==
        fx::ReactionSound{.actor = 12, .variant = 3, .last_variant = 1});
  CHECK(fx::SourceReactionSound(endpoints, 0, 2, draws.Draw()) ==
        fx::ReactionSound{.actor = 12, .variant = 0, .last_variant = 2});
  CHECK_FALSE(fx::SourceReactionSound({.emit_from = PointEndpoint(5, kPoint)},
                                      9, 0, draws.Draw())
                  .has_value());
  CHECK(draws.Taken() == 0);
}

TEST_CASE("Random reaction variants redraw until they differ from the last",
          "[timeline-commands]") {
  const CommandEndpoints endpoints{.emit_from = ActorEndpoint(12)};
  Draws draws({4, 7, 0x7FFF, 5});
  CHECK(fx::SourceReactionSound(endpoints, 4, 1, draws.Draw()) ==
        fx::ReactionSound{.actor = 12, .variant = 2, .last_variant = 2});
  CHECK(draws.Taken() == 4);

  Draws first({0x1234});
  CHECK(fx::SourceReactionSound(endpoints, 0xFF, 2, first.Draw()) ==
        fx::ReactionSound{.actor = 12, .variant = 1, .last_variant = 1});
  CHECK(first.Taken() == 1);

  Draws none({});
  CHECK_FALSE(fx::SourceReactionSound({.emit_from = PointEndpoint(5, kPoint)},
                                      4, 0, none.Draw())
                  .has_value());
  CHECK(none.Taken() == 0);
}

TEST_CASE("Live timeline commands refuse a missing instance",
          "[timeline-commands]") {
  const u_char last = Effects_lastReactionSoundVariant;
  fx::LiveApplyTargetActionImpact(nullptr);
  fx::LiveApplySourceActionImpact(nullptr);
  fx::LiveActivateTargetRoomTile(nullptr);
  fx::LiveSignalSourceActorFromTimeline(nullptr);
  fx::LiveSetSourceWeaponBrightness(nullptr, 1);
  fx::LivePlaySourceActorReactionSound(nullptr, 4);
  fx::LiveActivateTargetGimmick(nullptr);
  fx::LivePrepareTargetDefenseTiming(nullptr);
  CHECK(Effects_lastReactionSoundVariant == last);
}

TEST_CASE("Live reaction sound skips a non-actor emitter",
          "[timeline-commands]") {
  static Effects_EffectInstance instance{};
  instance.context.emitFrom.type = fx::kRoomPointEndpoint;
  const u_char last = Effects_lastReactionSoundVariant;
  fx::LivePlaySourceActorReactionSound(&instance, 4);
  CHECK(Effects_lastReactionSoundVariant == last);
}

namespace {
// Records the effect resource, trail and room object calls in order.
class RecordingEffectHost final : public fx::TimelineEffectHost {
 public:
  void SaveResourceContext() override { calls.emplace_back("save"); }
  void RestoreResourceContext() override { calls.emplace_back("restore"); }
  void SpawnBoneTrail(const fx::WeaponTrail& trail) override {
    calls.emplace_back("spawn");
    trails.push_back(trail);
  }
  void FadeBoneTrails() override { calls.emplace_back("fade"); }
  void FreezeRoomObjects() override { calls.emplace_back("freeze"); }

  std::vector<std::string> calls;
  std::vector<fx::WeaponTrail> trails;
};

// Records the commands performed and answers AdvanceKey with `more_keys`.
class RecordingControlHost final : public fx::ActionControlHost {
 public:
  explicit RecordingControlHost(bool more_keys) : more_keys_(more_keys) {}

  void Perform(const fx::ActionControl& control) override {
    performed.push_back(control);
  }
  bool AdvanceKey() override {
    ++advances;
    return more_keys_;
  }

  std::vector<fx::ActionControl> performed;
  int advances = 0;

 private:
  bool more_keys_;
};

constexpr std::uint16_t Timing(std::uint16_t command, std::uint16_t frame) {
  return static_cast<std::uint16_t>((command << 9) | frame);
}

Effects_TransformMotionView& MotionOf(Effects_EffectInstance& instance,
                                      std::size_t slot) {
  return *reinterpret_cast<Effects_TransformMotionView*>(
      &instance.transforms[slot]);
}

using InstanceBytes = std::array<std::byte, sizeof(Effects_EffectInstance)>;
InstanceBytes BytesOf(const Effects_EffectInstance& instance) {
  InstanceBytes bytes{};
  std::memcpy(bytes.data(), &instance, bytes.size());
  return bytes;
}

// An action-control track (kind 12) over `keys`, at its first key.
Effects_TimelineTrack ControlTrack(std::span<Effects_ActionControlKey> keys) {
  Effects_TimelineTrack track{};
  track.keys = reinterpret_cast<u_long*>(keys.data());
  track.header.kind = 12;
  track.header.keyCount = static_cast<u_char>(keys.size());
  return track;
}
}  // namespace

TEST_CASE("Weapon trails spawn inside the action's resource context",
          "[timeline-commands]") {
  RecordingEffectHost host;
  fx::SpawnTransformWeaponTrail({.actor = 7, .bone = 3}, 47, host);
  fx::SpawnTransformWeaponTrail({.actor = 1, .bone = 0}, 0xFF, host);
  CHECK(host.calls == std::vector<std::string>{"save", "spawn", "restore",
                                               "save", "spawn", "restore"});
  CHECK(host.trails == std::vector<fx::WeaponTrail>{
                           {.actor = 7, .bone = 3, .color = 7, .texture = 4},
                           {.actor = 1, .bone = 0, .color = 5, .texture = 25}});

  RecordingEffectHost others;
  fx::FadeActionWeaponTrails(others);
  fx::PauseMovingRoomObjects(others);
  CHECK(others.calls == std::vector<std::string>{"fade", "freeze"});
}

TEST_CASE("Transform and timeline flag commands change only their bits",
          "[timeline-commands]") {
  CHECK(fx::ResumeTransformMotion(0x8003) == 0x8007);
  CHECK(fx::ResumeTransformMotion(fx::kTransformFrameAdvances) ==
        fx::kTransformFrameAdvances);
  CHECK(fx::InvalidateTransformAttachments(0xFFFF) == 0xFFFC);
  CHECK(fx::InvalidateTransformAttachments(0x0004) == 0x0004);
  CHECK(fx::HoldTimelineFrame(0x02) == 0x03);
  CHECK(fx::HoldTimelineFrame(0xFF) == 0xFF);

  CHECK(fx::TransformSlotOf(0) == 0U);
  CHECK(fx::TransformSlotOf(15) == 15U);
  CHECK_FALSE(fx::TransformSlotOf(16).has_value());
  CHECK_FALSE(fx::TransformSlotOf(0xFF).has_value());
}

TEST_CASE("Action-control keys run once due and then advance the track",
          "[timeline-commands]") {
  const fx::ActionControlKey focus{
      .timing = Timing(13, 0x40), .operand0 = 2, .operand1 = 9};
  RecordingControlHost waiting(true);
  CHECK(fx::TickActionControlTrack(focus, 0x3F, waiting));
  CHECK(waiting.performed.empty());
  CHECK(waiting.advances == 0);

  RecordingControlHost due(false);
  CHECK_FALSE(fx::TickActionControlTrack(focus, 0x40, due));
  CHECK(due.performed == std::vector<fx::ActionControl>{
                             {.command = fx::ActionControlCommand::kFocusCamera,
                              .operand0 = 2,
                              .operand1 = 9}});
  CHECK(due.advances == 1);
}

TEST_CASE("Action-control keys past the commands or the frame only wait",
          "[timeline-commands]") {
  // A negative timeline frame never matches the key's nine frame bits.
  RecordingControlHost negative(true);
  CHECK(fx::TickActionControlTrack({.timing = Timing(0, 0x1FF)}, -1, negative));
  CHECK(negative.advances == 0);

  // Commands from 16 on do nothing but still advance the track.
  RecordingControlHost unknown(true);
  CHECK(fx::TickActionControlTrack({.timing = Timing(16, 5)}, 5, unknown));
  CHECK(fx::TickActionControlTrack({.timing = Timing(127, 5)}, 5, unknown));
  CHECK(unknown.performed.empty());
  CHECK(unknown.advances == 2);
}

TEST_CASE("Live transform commands update the instance's own transforms",
          "[timeline-commands]") {
  // #388: the copied commands viewed the instance with the PS1 header size,
  // so they wrote the registers and the previous transform instead.
  static Effects_EffectInstance instance{};
  std::ranges::fill(instance.registers, 0x5A);
  MotionOf(instance, 4).flags = 0x8003;
  fx::LiveResumeEffectTransformMotion(&instance, 0);
  fx::LiveResumeEffectTransformMotion(&instance, 4);
  fx::LiveInvalidateEffectTransformAttachments(&instance, 4);
  CHECK(MotionOf(instance, 0).flags == fx::kTransformFrameAdvances);
  CHECK(MotionOf(instance, 4).flags == 0x8004);
  CHECK(MotionOf(instance, 3).flags == 0);
  CHECK(std::ranges::all_of(instance.registers,
                            [](char value) { return value == 0x5A; }));

  const InstanceBytes before = BytesOf(instance);
  fx::LiveResumeEffectTransformMotion(&instance, 16);
  fx::LiveInvalidateEffectTransformAttachments(&instance, 0xFF);
  fx::LiveSpawnTransformWeaponTrail(&instance, 16, 47);
  fx::LiveResumeEffectTransformMotion(nullptr, 0);
  fx::LiveInvalidateEffectTransformAttachments(nullptr, 0);
  fx::LiveSpawnTransformWeaponTrail(nullptr, 0, 47);
  fx::LiveHoldEffectTimelineFrame(nullptr);
  fx::LiveHoldEffectTimelineFrame(&instance);
  CHECK(BytesOf(instance) == before);
}

TEST_CASE("Live action-control track runs due keys on the instance",
          "[timeline-commands]") {
  static Effects_EffectTimeline timeline;
  static Effects_EffectInstance instance{};
  static std::array<Effects_ActionControlKey, 2> keys{{
      {.frame = Timing(6, 12), .opaque2 = 0, .opaque3 = 0},
      {.frame = Timing(5, 12), .opaque2 = 4, .opaque3 = 0},
  }};
  timeline.frame = 12;
  instance.context.timeline = &timeline;
  Effects_TimelineTrack track = ControlTrack(keys);

  CHECK(fx::LiveTickActionControlTrack(&instance, &track) == 1);
  CHECK((timeline.flags & fx::kTimelineFrameHeld) != 0);
  CHECK(track.keyIndex == 1);
  // The last key resumes transform 4 and ends the track.
  CHECK(fx::LiveTickActionControlTrack(&instance, &track) == 0);
  CHECK(MotionOf(instance, 4).flags == fx::kTransformFrameAdvances);
  CHECK(track.keyIndex == 1);

  timeline.frame = 13;
  CHECK(fx::LiveTickActionControlTrack(&instance, &track) == 1);
}

TEST_CASE("Live action-control track refuses a missing or unread key",
          "[timeline-commands]") {
  static Effects_EffectTimeline timeline;
  static Effects_EffectInstance instance{};
  static std::array<Effects_ActionControlKey, 2> keys{{
      {.frame = Timing(6, 0), .opaque2 = 0, .opaque3 = 0},
      {.frame = Timing(6, 0), .opaque2 = 0, .opaque3 = 0},
  }};
  instance.context.timeline = &timeline;
  Effects_TimelineTrack track = ControlTrack(keys);

  track.keyIndex = -1;
  CHECK(fx::LiveTickActionControlTrack(&instance, &track) == 0);
  track.keyIndex = 2;
  CHECK(fx::LiveTickActionControlTrack(&instance, &track) == 0);
  CHECK(fx::LiveTickActionControlTrack(&instance, nullptr) == 0);
  CHECK(fx::LiveTickActionControlTrack(nullptr, &track) == 0);
  instance.context.timeline = nullptr;
  track.keyIndex = 0;
  CHECK(fx::LiveTickActionControlTrack(&instance, &track) == 0);
  track.keys = nullptr;
  instance.context.timeline = &timeline;
  CHECK(fx::LiveTickActionControlTrack(&instance, &track) == 0);
  CHECK(timeline.flags == 0);
}
