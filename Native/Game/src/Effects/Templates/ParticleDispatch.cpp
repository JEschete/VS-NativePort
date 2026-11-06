// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/ParticleDispatch.hpp"

#include <cstdint>
#include <optional>

#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/ParticleCore.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;

// decomp: particleDispatch.h:14-44, the slot state and its control points.
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

// decomp: particleDispatch.h:46-80, one tick; false once the lifetime runs
// out. The stages work on a frame the host publishes after them.
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

// decomp: particleDispatch.h:82-84, the set ends on its next tick.
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
// (src/EFFECT/particleDispatch.h:3).
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
      // decomp: particleDispatch.h:86-89.
      host.Release();
      return false;
    default:
      return true;
  }
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG045.BIN func_800FB320 (src/EFFECT/PLG045.BIN/110.c:5).
bool Plg045::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG057.BIN func_800FB4E0 (src/EFFECT/PLG057.BIN/2D0.c:7).
bool Plg057::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG065.BIN func_800FD6C0 (src/EFFECT/PLG065.BIN/F8.c:40).
bool Plg065::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG082.BIN func_800FB338 (src/EFFECT/PLG082.BIN/128.c:13).
bool Plg082::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG087.BIN func_800FF4E8 (src/EFFECT/PLG087.BIN/1F0.c:52).
bool Plg087::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG114.BIN func_800FB418 (src/EFFECT/PLG114.BIN/208.c:7).
bool Plg114::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG114.BIN func_800FCA2C (src/EFFECT/PLG114.BIN/208.c:35).
bool Plg114::ParticleDispatch2(Templates::ParticleCommand command,
                               std::int32_t packed,
                               Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatch2Options, command, packed,
                                     host);
}

// decomp: PLG146.BIN func_800FC8C4 (src/EFFECT/PLG146.BIN/1D8.c:31).
bool Plg146::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG147.BIN func_800FB89C (src/EFFECT/PLG147.BIN/F8.c:27).
bool Plg147::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG152.BIN func_800FBE88 (src/EFFECT/PLG152.BIN/128.c:16).
bool Plg152::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG153.BIN func_800FCA50 (src/EFFECT/PLG153.BIN/110.c:17).
bool Plg153::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG156.BIN func_800FB338 (src/EFFECT/PLG156.BIN/128.c:7).
bool Plg156::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG168.BIN func_800FCE90 (src/EFFECT/PLG168.BIN/1F0.c:33).
bool Plg168::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG224.BIN func_800FAC78 (src/EFFECT/PLG224.BIN/F8.c:23).
bool Plg224::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG255.BIN func_800FCE78 (src/EFFECT/PLG255.BIN/1D8.c:33).
bool Plg255::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG263.BIN func_800FDA1C (src/EFFECT/PLG263.BIN/110.c:349).
bool Plg263::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG265.BIN func_800FBD60 (src/EFFECT/PLG265.BIN/2E8.c:27).
bool Plg265::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG265.BIN func_800FFB04 (src/EFFECT/PLG265.BIN/2E8.c:45).
bool Plg265::ParticleDispatch2(Templates::ParticleCommand command,
                               std::int32_t packed,
                               Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatch2Options, command, packed,
                                     host);
}

// decomp: PLG266.BIN func_800FB4F8 (src/EFFECT/PLG266.BIN/2E8.c:7).
bool Plg266::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG266.BIN func_800FCB0C (src/EFFECT/PLG266.BIN/2E8.c:35).
bool Plg266::ParticleDispatch2(Templates::ParticleCommand command,
                               std::int32_t packed,
                               Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatch2Options, command, packed,
                                     host);
}

// decomp: PLG300.BIN func_800FBF94 (src/EFFECT/PLG300.BIN/110.c:17).
bool Plg300::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}

// decomp: PLG301.BIN func_800FB320 (src/EFFECT/PLG301.BIN/110.c:13).
bool Plg301::ParticleDispatch(Templates::ParticleCommand command,
                              std::int32_t packed,
                              Templates::ParticleSetHost& host) {
  return Templates::ParticleDispatch(kParticleDispatchOptions, command, packed,
                                     host);
}
}  // namespace VS::Effects
