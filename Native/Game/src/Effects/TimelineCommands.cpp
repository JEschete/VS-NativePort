// Hand-maintained (tools/port/owned.csv).
#include "Effects/TimelineCommands.hpp"

#include <cstddef>
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

// decomp: func_800D6388 (src/BATTLE/BATTLE.PRG/5BF94.c:7207)
ActionImpact TargetActionImpact(const CommandEndpoints& endpoints) {
  return ImpactOf(endpoints.target);
}

// decomp: func_800D63D0 (src/BATTLE/BATTLE.PRG/5BF94.c:7217)
ActionImpact SourceActionImpact(const CommandEndpoints& endpoints) {
  return ImpactOf(endpoints.source);
}

// decomp: func_800D6418 (src/BATTLE/BATTLE.PRG/5BF94.c:7227)
std::optional<EndpointPoint> TargetRoomTile(const CommandEndpoints& endpoints) {
  return RoomPointOf(endpoints.target);
}

// decomp: func_800D6574 (src/BATTLE/BATTLE.PRG/5BF94.c:7253)
std::optional<std::uint8_t> TimelineSourceActorToSignal(
    const CommandEndpoints& endpoints) {
  return ActorOf(endpoints.source);
}

// decomp: func_800D65D8 (src/BATTLE/BATTLE.PRG/5BF94.c:7266)
std::optional<WeaponBrightness> SourceWeaponBrightness(
    const CommandEndpoints& endpoints, std::uint8_t brightness) {
  const auto actor = ActorOf(endpoints.emit_from);
  if (!actor) return std::nullopt;
  return WeaponBrightness{
      .actor = *actor, .enabled = brightness != 0, .brightness = brightness};
}

// decomp: func_800D6628 (src/BATTLE/BATTLE.PRG/5BF94.c:7279)
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

// decomp: func_800D66CC (src/BATTLE/BATTLE.PRG/5BF94.c:7295)
std::optional<EndpointPoint> TargetGimmick(const CommandEndpoints& endpoints) {
  return RoomPointOf(endpoints.target);
}

// decomp: func_800D6860 (src/BATTLE/BATTLE.PRG/5BF94.c:7329)
std::optional<std::uint8_t> DefenseTimingTarget(
    const CommandEndpoints& endpoints) {
  return ActorOf(endpoints.emit_to);
}

std::optional<std::size_t> TransformSlotOf(std::uint8_t operand) {
  if (operand >= kEffectTransformSlots) return std::nullopt;
  return operand;
}

// decomp: func_800D6448 (src/BATTLE/BATTLE.PRG/5BF94.c:7234)
void SpawnTransformWeaponTrail(TransformAnchor anchor,
                               std::uint8_t packed_style,
                               TimelineEffectHost& host) {
  host.SaveResourceContext();
  host.SpawnBoneTrail(
      {.actor = anchor.actor,
       .bone = anchor.bone,
       .color = static_cast<std::uint8_t>(packed_style % kTrailStyleBase),
       .texture = static_cast<std::uint8_t>(packed_style / kTrailStyleBase)});
  host.RestoreResourceContext();
}

// decomp: func_800D64E4 (src/BATTLE/BATTLE.PRG/5BF94.c:7241)
void FadeActionWeaponTrails(TimelineEffectHost& host) { host.FadeBoneTrails(); }

// decomp: func_800D6508 (src/BATTLE/BATTLE.PRG/5BF94.c:7243)
std::uint16_t ResumeTransformMotion(std::uint16_t flags) {
  return static_cast<std::uint16_t>(flags | kTransformFrameAdvances);
}

// decomp: func_800D6538 (src/BATTLE/BATTLE.PRG/5BF94.c:7249)
std::uint8_t HoldTimelineFrame(std::uint8_t flags) {
  return static_cast<std::uint8_t>(flags | kTimelineFrameHeld);
}

// decomp: func_800D6554 (src/BATTLE/BATTLE.PRG/5BF94.c:7251)
void PauseMovingRoomObjects(TimelineEffectHost& host) {
  host.FreezeRoomObjects();
}

// decomp: func_800D65A8 (src/BATTLE/BATTLE.PRG/5BF94.c:7260)
std::uint16_t InvalidateTransformAttachments(std::uint16_t flags) {
  constexpr std::uint16_t kResolved =
      kTransformAttachmentResolved | kTransformTargetResolved;
  return static_cast<std::uint16_t>(flags & ~kResolved);
}

// decomp: func_800D6894 (src/BATTLE/BATTLE.PRG/5BF94.c:7336)
bool TickActionControlTrack(const ActionControlKey& key,
                            std::int16_t timeline_frame,
                            ActionControlHost& host) {
  if ((key.timing & kActionControlFrameMask) != timeline_frame) return true;
  const auto command = key.timing >> kActionControlCommandShift;
  if (command < kActionControlCommands) {
    host.Perform({.command = static_cast<ActionControlCommand>(command),
                  .operand0 = key.operand0,
                  .operand1 = key.operand1});
  }
  return host.AdvanceKey();
}
}  // namespace VS::Effects
