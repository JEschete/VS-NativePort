#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Effects/TimelineCommands.hpp"
#include "Effects/TimelineCommandsLive.hpp"
extern "C" {
#include "Effects/Runtime.h"
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
