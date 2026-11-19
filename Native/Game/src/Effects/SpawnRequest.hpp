#pragma once
// The request that spawns an effect instance and the endpoints the instance
// keeps from it: InitEffectInstance (EffectInstancesLive.cpp) takes the
// spawn's parent, script and identity and copies its four endpoints into the
// instance's context. The lifecycle rules are in Effects/EffectInstances.hpp.

#include <cstdint>

#include "Effects/ActionRequest.hpp"

struct Effects_EffectInstance;

namespace VS::Effects {
struct LiveRendererSlot;
struct LiveEffectTimeline;

// decomp: D_800F53B8_t3 (src/BATTLE/BATTLE.PRG/battleEffect.h:325): what an
// effect instance plays between, the timeline and renderers it runs, and the
// effect it belongs to.
struct LiveEffectInstanceTargets {
  LiveActionEffectTarget source;
  // Where the effect starts: the source, or the target when it is reversed.
  LiveActionEffectTarget emit_from;
  LiveActionEffectTarget target;
  // Where the effect ends: the target, or the source when it is reversed.
  LiveActionEffectTarget emit_to;
  LiveEffectTimeline* timeline;
  // The instance's renderers, newest first.
  LiveRendererSlot* renderer;
  std::int16_t effect_id;
  // A secondary instance's SecondaryEffectPhase, as stored.
  std::int16_t phase;
  std::int32_t parameter;
};

// decomp: D_800F53B8_t2 (src/BATTLE/BATTLE.PRG/5BF94.c:115): the request
// InitEffectInstance starts an instance from. The reserved bytes are never
// read.
struct LiveEffectSpawnRequest {
  LiveActionEffectTarget* source;
  LiveActionEffectTarget* emit_from;
  LiveActionEffectTarget* target;
  LiveActionEffectTarget* emit_to;
  std::int32_t priority;
  char* script;
  std::int32_t script_offset;
  Effects_EffectInstance* parent;
  std::uint8_t token;
  std::uint8_t reserved21;
  std::uint8_t reserved22;
  std::uint8_t reserved23;
  std::int32_t reserved24;
};
}  // namespace VS::Effects
