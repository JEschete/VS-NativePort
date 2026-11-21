// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/ParticleDispatch.hpp"

#include <cstdint>
#include <optional>

#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/ParticleCore.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;

// decomp: src/EFFECT/particleDispatch.h:14-44, the slot state and its control
// points.
bool CreateSet(const ParticleDispatchOptions& options, std::int32_t packed,
               ParticleSetHost& host) {
  const ParticleSetPlan plan = PlanParticleSet(packed);
  const std::optional<ParticleSetDefinition> definition =
      host.Definition(plan.definition_index);
  if (!definition) {
    return true;
  }
  host.Create(
      {.lifetime = plan.lifetime, .definition_index = plan.definition_index},
      ParticleSetCapacity(definition->interval), options.texturing);
  return true;
}

void RunStage(ParticleStage stage, ParticleSetFrame& frame,
              ParticleSetHost& host) {
  if (stage != nullptr) {
    stage(frame, host);
  }
}

// decomp: src/EFFECT/particleDispatch.h:46-80, one tick; false once the
// lifetime runs out. The stages work on a frame the host publishes after them.
bool TickSet(const ParticleDispatchOptions& options, ParticleSetHost& host) {
  const std::optional<ParticleSetState> state = host.State();
  if (!state) {
    return false;
  }
  const std::optional<ParticleTickInputs> inputs =
      host.BeginTick(state->definition_index);
  if (!inputs) {
    host.Release();
    return false;
  }
  ParticleSetFrame frame{.inputs = *inputs,
                         .state = *state,
                         .scratch = host.LoadScratch(),
                         .points = host.LoadPoints(),
                         .texturing = options.texturing};
  const ParticleSetDefinition& definition = frame.inputs.definition;
  frame.scratch.flags = definition.flags & kParticleDefinitionFlagsMask;
  frame.state.count = ParticleSetCount(SampleParticleScalar(
      definition.interval, definition.curves.interval, frame.state.age, host));
  if (frame.state.cooldown <= 0) {
    RunStage(options.origin, frame, host);
    RunStage(options.target, frame, host);
    frame.state.cooldown = definition.cooldown;
  }
  frame.state.cooldown = math::WrappingSubtract(frame.state.cooldown, 1);
  RunStage(options.control, frame, host);
  RunStage(options.render, frame, host);
  frame.state.age = math::WrappingAdd(frame.state.age, 1);
  bool alive = true;
  if (frame.state.lifetime != 0) {
    --frame.state.lifetime;
    alive = frame.state.lifetime != 0;
  }
  host.StorePoints(frame.points);
  host.PublishScratch(frame.scratch);
  host.StoreState(frame.state);
  if (!alive) {
    host.Release();
  }
  return alive;
}

// decomp: src/EFFECT/particleDispatch.h:82-84, the set ends on its next tick.
bool ExpireSet(ParticleSetHost& host) {
  std::optional<ParticleSetState> state = host.State();
  if (state) {
    state->lifetime = 1;
    host.StoreState(*state);
  }
  return true;
}
}  // namespace

// decomp: EFFECT VS_PARTICLE_DISPATCH_FUNCTION
// (src/EFFECT/particleDispatch.h:8).
bool ParticleDispatch(const ParticleDispatchOptions& options,
                      ParticleCommand command, std::int32_t packed,
                      ParticleSetHost& host) {
  switch (command) {
    case ParticleCommand::kCreate:
      return CreateSet(options, packed, host);
    case ParticleCommand::kTick:
      return TickSet(options, host);
    case ParticleCommand::kExpire:
      return ExpireSet(host);
    case ParticleCommand::kStop:
      // decomp: src/EFFECT/particleDispatch.h:8-88.
      host.Release();
      return false;
    default:
      return true;
  }
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG045.BIN vs_effect_lightningBolt_0
// (src/EFFECT/particleDispatch.h:8; was func_800FB320).
bool Plg045::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG057.BIN vs_effect_thunderBurstLv1_0
// (src/EFFECT/particleDispatch.h:8; was func_800FB4E0).
bool Plg057::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG065.BIN vs_effect_gaeaStrikeLv1_3
// (src/EFFECT/particleDispatch.h:8; was func_800FD6C0).
bool Plg065::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG082.BIN vs_effect_thunderbolt_0 (src/EFFECT/particleDispatch.h:8;
// was func_800FB338).
bool Plg082::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG087.BIN vs_effect_apocalypse_4 (src/EFFECT/particleDispatch.h:8;
// was func_800FF4E8).
bool Plg087::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG114.BIN vs_effect_numbingNeedle_0
// (src/EFFECT/particleDispatch.h:8; was func_800FB418).
bool Plg114::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG114.BIN vs_effect_numbingNeedle_1
// (src/EFFECT/particleDispatch.h:8; was func_800FCA2C).
bool Plg114::ParticleDispatch2(Templates::ParticleCommand command,
                               std::int32_t packed,
                               Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatch2Options, command, packed,
                                     host);
}

// decomp: PLG146.BIN vs_effect_blackNebula_2 (src/EFFECT/particleDispatch.h:8;
// was func_800FC8C4).
bool Plg146::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG147.BIN vs_effect_bearClaw_1 (src/EFFECT/particleDispatch.h:8; was
// func_800FB89C).
bool Plg147::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG152.BIN vs_effect_riskbreak_1 (src/EFFECT/particleDispatch.h:8;
// was func_800FBE88).
bool Plg152::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG153.BIN vs_effect_gravisAether_2 (src/EFFECT/particleDispatch.h:8;
// was func_800FCA50).
bool Plg153::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG156.BIN vs_effect_quickshock_0 (src/EFFECT/particleDispatch.h:8;
// was func_800FB338).
bool Plg156::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG168.BIN vs_effect_vertigo_1 (src/EFFECT/particleDispatch.h:8; was
// func_800FCE90).
bool Plg168::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG224.BIN vs_effect_plg224_0 (src/EFFECT/particleDispatch.h:8; was
// func_800FAC78).
bool Plg224::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG255.BIN vs_effect_plg255_1 (src/EFFECT/particleDispatch.h:8; was
// func_800FCE78).
bool Plg255::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG263.BIN vs_effect_plg263_3 (src/EFFECT/particleDispatch.h:8; was
// func_800FDA1C).
bool Plg263::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG265.BIN vs_effect_plg265_1 (src/EFFECT/particleDispatch.h:8; was
// func_800FBD60).
bool Plg265::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG265.BIN vs_effect_plg265_3 (src/EFFECT/particleDispatch.h:8; was
// func_800FFB04).
bool Plg265::ParticleDispatch2(Templates::ParticleCommand command,
                               std::int32_t packed,
                               Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatch2Options, command, packed,
                                     host);
}

// decomp: PLG266.BIN vs_effect_plg266_0 (src/EFFECT/particleDispatch.h:8; was
// func_800FB4F8).
bool Plg266::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG266.BIN vs_effect_plg266_1 (src/EFFECT/particleDispatch.h:8; was
// func_800FCB0C).
bool Plg266::ParticleDispatch2(Templates::ParticleCommand command,
                               std::int32_t packed,
                               Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatch2Options, command, packed,
                                     host);
}

// decomp: PLG300.BIN vs_effect_plg300_1 (src/EFFECT/particleDispatch.h:8; was
// func_800FBF94).
bool Plg300::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG301.BIN vs_effect_plg301_0 (src/EFFECT/particleDispatch.h:8; was
// func_800FB320).
bool Plg301::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}
}  // namespace VS::Effects
