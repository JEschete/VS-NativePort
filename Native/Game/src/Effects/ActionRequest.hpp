#pragma once
// An action's effect request as the live bindings hand it to the effect
// runtime: its source, targets, other actors and focus point, and each
// endpoint's position or actor part (EffectRuntimeLive.cpp begins the
// primary and secondary effects from it). The request rules are in
// Effects/EffectRuntime.hpp, Effects/ActionEffectCallbacks.hpp and
// Effects/SecondaryEffectRequests.hpp.

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

#include "Core/Gte.h"
#include "Effects/ActionEffectCallbacks.hpp"
#include "Effects/EffectRuntime.hpp"

namespace VS::Effects {
// decomp: ActionEffectTarget.value.actor (src/BATTLE/BATTLE.PRG/
// battleEffect.h:15-20): an actor endpoint's actor and part, kept in the
// first four bytes of the endpoint's position.
struct LiveActorEndpoint {
  std::uint8_t actor;
  // The event actor slot a child track's next target names
  // (TimelinePlaybackLive.cpp).
  std::uint8_t event_actor_slot;
  std::uint8_t part;
  std::uint8_t reserved3;
};

// decomp: ActionEffectTarget (src/BATTLE/BATTLE.PRG/battleEffect.h:11): one
// endpoint of an action effect. An endpoint on an actor (kActorBoneEndpoint)
// keeps its LiveActorEndpoint where a point's x and y are; any other names
// `position`.
struct LiveActionEffectTarget {
  std::uint16_t type;
  std::uint16_t color_preset;
  SVECTOR position;

  // The actor endpoint in `position`'s x and y.
  [[nodiscard]] LiveActorEndpoint Actor() const {
    return std::bit_cast<LiveActorEndpoint>(
        std::array<std::int16_t, 2>{position.vx, position.vy});
  }
  // Stores `actor` in `position`'s x and y.
  void SetActor(const LiveActorEndpoint& actor) {
    const auto [vx, vy] = std::bit_cast<std::array<std::int16_t, 2>>(actor);
    position.vx = vx;
    position.vy = vy;
  }
};

// decomp: ActionEffectRequest (src/BATTLE/BATTLE.PRG/battleEffect.h:25): an
// action's effect request. `count` targets and `other_count` other actors
// are in use; `position` is the action's focus point, the effect origin the
// templates and transforms read.
struct LiveActionEffectRequest {
  std::uint16_t effect_id;
  std::uint8_t count;
  std::uint8_t other_count;
  LiveActionEffectTarget source;
  std::array<LiveActionEffectTarget, kActionEffectTargets> targets;
  std::array<std::uint8_t, kPrimaryEffectOthers> others;
  SVECTOR position;
};

namespace layout {
// The actor endpoint overlays a point's x and y.
static_assert(sizeof(LiveActorEndpoint) == 2 * sizeof(std::int16_t));
static_assert(sizeof(LiveActionEffectTarget) ==
              (2 * sizeof(std::uint16_t)) + sizeof(SVECTOR));
// The request's size as the module bindings place the globals
// (0xE8 bytes, battleEffect.h:25).
inline constexpr std::size_t kActionEffectRequestBytes = 0xE8;
static_assert(sizeof(LiveActionEffectRequest) == kActionEffectRequestBytes);
}  // namespace layout
}  // namespace VS::Effects
