#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <functional>
#include <optional>

#include "Effects/Transforms.hpp"

// Typed effect timeline action-control commands (BATTLE.PRG 5BF94.c:8213-8340).
// Each reads the effect instance's endpoints and selects what the command
// applies to; TimelineCommandsLive.hpp performs it.
namespace VS::Effects {
// An endpoint on a room tile or room object (Effects_ActionEffectTarget type
// 5).
inline constexpr std::uint16_t kRoomPointEndpoint = 5;
// Reaction sound variants below this play as given; the others pick one of the
// random variants.
inline constexpr std::uint8_t kFixedReactionVariants = 4;
inline constexpr std::int32_t kRandomReactionVariants = 3;

// One draw from the BIOS rand() sequence.
using RandomDraw = std::function<std::int32_t()>;

// Effects_ActionEffectTarget.value.position; `pad` names the actor or room
// object the point belongs to.
struct EndpointPoint {
  std::int16_t x{};
  std::int16_t y{};
  std::int16_t z{};
  std::int16_t pad{};

  friend bool operator==(const EndpointPoint&, const EndpointPoint&) = default;
};

// An action effect endpoint as the commands read it: `actor` is
// value.actor.actor, which overlays the low byte of the point's x.
struct CommandEndpoint {
  std::uint16_t type{};
  std::uint8_t actor{};
  EndpointPoint point{};
};

// The effect instance's endpoints (Effects_EffectInstanceTargets).
struct CommandEndpoints {
  CommandEndpoint source{};
  CommandEndpoint emit_from{};
  CommandEndpoint target{};
  CommandEndpoint emit_to{};
};

// An action impact's arguments: an actor endpoint names the actor alone; any
// other names the actor in the point's pad, with the point.
struct ActionImpact {
  std::int32_t actor{};
  std::optional<EndpointPoint> point;

  friend bool operator==(const ActionImpact&, const ActionImpact&) = default;
};

// A weapon brightness override for an actor; `enabled` is false (and
// `brightness` 0) when the command clears it.
struct WeaponBrightness {
  std::uint8_t actor{};
  bool enabled{};
  std::uint8_t brightness{};

  friend bool operator==(const WeaponBrightness&,
                         const WeaponBrightness&) = default;
};

// A reaction sound for an actor and the random variant the next random pick
// must avoid.
struct ReactionSound {
  std::uint8_t actor{};
  std::uint8_t variant{};
  std::uint8_t last_variant{};

  friend bool operator==(const ReactionSound&, const ReactionSound&) = default;
};

// decomp: func_800D6388 (src/BATTLE/BATTLE.PRG/5BF94.c:8213)
[[nodiscard]] ActionImpact TargetActionImpact(
    const CommandEndpoints& endpoints);
// decomp: func_800D63D0 (src/BATTLE/BATTLE.PRG/5BF94.c:8223)
[[nodiscard]] ActionImpact SourceActionImpact(
    const CommandEndpoints& endpoints);
// decomp: func_800D6418 (src/BATTLE/BATTLE.PRG/5BF94.c:8233): the room tile
// whose trap the target disables.
[[nodiscard]] std::optional<EndpointPoint> TargetRoomTile(
    const CommandEndpoints& endpoints);
// decomp: func_800D6574 (src/BATTLE/BATTLE.PRG/5BF94.c:8259)
[[nodiscard]] std::optional<std::uint8_t> TimelineSourceActorToSignal(
    const CommandEndpoints& endpoints);
// decomp: func_800D65D8 (src/BATTLE/BATTLE.PRG/5BF94.c:8272): a brightness of
// 0 clears the override.
[[nodiscard]] std::optional<WeaponBrightness> SourceWeaponBrightness(
    const CommandEndpoints& endpoints, std::uint8_t brightness);
// decomp: func_800D6628 (src/BATTLE/BATTLE.PRG/5BF94.c:8285): variants from
// kFixedReactionVariants on draw until the pick differs from `last_variant`.
[[nodiscard]] std::optional<ReactionSound> SourceReactionSound(
    const CommandEndpoints& endpoints, std::uint8_t variant,
    std::uint8_t last_variant, const RandomDraw& draw);
// decomp: func_800D66CC (src/BATTLE/BATTLE.PRG/5BF94.c:8301): the room object
// the target unlocks.
[[nodiscard]] std::optional<EndpointPoint> TargetGimmick(
    const CommandEndpoints& endpoints);
// decomp: func_800D6860 (src/BATTLE/BATTLE.PRG/5BF94.c:8335): the actor whose
// defense ability timing window opens.
[[nodiscard]] std::optional<std::uint8_t> DefenseTimingTarget(
    const CommandEndpoints& endpoints);
}  // namespace VS::Effects
