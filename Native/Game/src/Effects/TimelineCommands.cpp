// Hand-maintained (tools/port/owned.csv).
#include "Effects/TimelineCommands.hpp"

#include <cstdint>
#include <optional>

#include "Effects/Transforms.hpp"

namespace VS::Effects {
namespace {
[[nodiscard]] constexpr std::optional<std::uint8_t> ActorOf(
    const CommandEndpoint& endpoint) {
  if (endpoint.type != kActorPartEndpoint) return std::nullopt;
  return endpoint.actor;
}

[[nodiscard]] constexpr std::optional<EndpointPoint> RoomPointOf(
    const CommandEndpoint& endpoint) {
  if (endpoint.type != kRoomPointEndpoint) return std::nullopt;
  return endpoint.point;
}

[[nodiscard]] constexpr ActionImpact ImpactOf(const CommandEndpoint& endpoint) {
  if (const auto actor = ActorOf(endpoint)) {
    return {.actor = *actor, .point = std::nullopt};
  }
  return {.actor = endpoint.point.pad, .point = endpoint.point};
}
}  // namespace

// decomp: func_800D6388 (src/BATTLE/BATTLE.PRG/5BF94.c:8213)
ActionImpact TargetActionImpact(const CommandEndpoints& endpoints) {
  return ImpactOf(endpoints.target);
}

// decomp: func_800D63D0 (src/BATTLE/BATTLE.PRG/5BF94.c:8223)
ActionImpact SourceActionImpact(const CommandEndpoints& endpoints) {
  return ImpactOf(endpoints.source);
}

// decomp: func_800D6418 (src/BATTLE/BATTLE.PRG/5BF94.c:8233)
std::optional<EndpointPoint> TargetRoomTile(const CommandEndpoints& endpoints) {
  return RoomPointOf(endpoints.target);
}

// decomp: func_800D6574 (src/BATTLE/BATTLE.PRG/5BF94.c:8259)
std::optional<std::uint8_t> TimelineSourceActorToSignal(
    const CommandEndpoints& endpoints) {
  return ActorOf(endpoints.source);
}

// decomp: func_800D65D8 (src/BATTLE/BATTLE.PRG/5BF94.c:8272)
std::optional<WeaponBrightness> SourceWeaponBrightness(
    const CommandEndpoints& endpoints, std::uint8_t brightness) {
  const auto actor = ActorOf(endpoints.emit_from);
  if (!actor) return std::nullopt;
  return WeaponBrightness{
      .actor = *actor, .enabled = brightness != 0, .brightness = brightness};
}

// decomp: func_800D6628 (src/BATTLE/BATTLE.PRG/5BF94.c:8285)
std::optional<ReactionSound> SourceReactionSound(
    const CommandEndpoints& endpoints, std::uint8_t variant,
    std::uint8_t last_variant, const RandomDraw& draw) {
  const auto actor = ActorOf(endpoints.emit_from);
  if (!actor) return std::nullopt;
  if (variant < kFixedReactionVariants) {
    return ReactionSound{
        .actor = *actor, .variant = variant, .last_variant = last_variant};
  }
  std::int32_t pick = 0;
  do {
    pick = draw() % kRandomReactionVariants;
  } while (pick == last_variant);
  const auto picked = static_cast<std::uint8_t>(pick);
  return ReactionSound{
      .actor = *actor, .variant = picked, .last_variant = picked};
}

// decomp: func_800D66CC (src/BATTLE/BATTLE.PRG/5BF94.c:8301)
std::optional<EndpointPoint> TargetGimmick(const CommandEndpoints& endpoints) {
  return RoomPointOf(endpoints.target);
}

// decomp: func_800D6860 (src/BATTLE/BATTLE.PRG/5BF94.c:8335)
std::optional<std::uint8_t> DefenseTimingTarget(
    const CommandEndpoints& endpoints) {
  return ActorOf(endpoints.emit_to);
}
}  // namespace VS::Effects
