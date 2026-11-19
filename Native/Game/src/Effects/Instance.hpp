#pragma once
// The effect instance record: one running effect script, in the game heap.
// Effects/EffectInstances.hpp has the lifecycle rules and the priority
// chains the instances form; EffectInstancesLive.cpp starts and links them.

#include <array>
#include <cstdint>

#include "Effects/EffectInstances.hpp"
#include "Effects/EffectScript.hpp"
#include "Effects/SpawnRequest.hpp"
#include "Effects/TransformsLiveState.hpp"

namespace VS::Effects {
struct LiveParticle;

// decomp: D_800F53B8_t (src/BATTLE/BATTLE.PRG/battleEffect.h:337)
struct LiveEffectInstance {
  // The next instance of its chain; a chain keeps ascending priority.
  LiveEffectInstance* next;
  // The instance that spawned this one, if any.
  LiveEffectInstance* parent;
  // Particles the instance owns.
  char particle_count;
  // The chain's sort key.
  char priority;
  // Offset of the next script byte.
  std::uint16_t script_offset;
  // The effect script's bytecode.
  char* script;
  std::array<char, kScriptRegisters> registers;
  // The effect token released when the instance is destroyed.
  std::uint32_t token : 8;
  // Bits OR'ed into the secondary update's busy result (2, or 4 for effect
  // 54).
  std::uint32_t busy_bits : 3;
  // Child instances still running.
  std::uint32_t child_count : 5;
  // The effect's frame, used to sample its curves.
  std::uint32_t frame : 16;
  // The first particle the instance owns.
  LiveParticle* particles;
  std::array<LiveTransformSlot, kEffectInstanceTransforms> transforms;
  // Targets, timeline, renderers, effect id and phase.
  LiveEffectInstanceTargets context;
};
}  // namespace VS::Effects
