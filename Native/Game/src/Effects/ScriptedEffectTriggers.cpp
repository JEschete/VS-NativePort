// Hand-maintained (tools/port/owned.csv).
#include "Effects/ScriptedEffectTriggers.hpp"

#include <cstddef>
#include <cstdint>

namespace VS::Effects {
PrimaryEffectRequest ScriptedActorEffectRequest(
    std::uint16_t effect_id, std::int32_t target,
    ScriptedEffectTriggerHost& host) {
  PrimaryEffectRequest request{
      .effect_id = effect_id,
      .source = {.kind = EffectEndpointKind::kPosition, .position = {}},
      .target = {.kind = EffectEndpointKind::kActor,
                 .actor = static_cast<std::uint8_t>(target),
                 .part = 0}};
  for (std::size_t slot = 0; slot < kScriptedEffectActorSlots; ++slot) {
    if (host.ActorLoaded(slot) && static_cast<std::int32_t>(slot) != target) {
      request.others[request.other_count] = static_cast<std::uint8_t>(slot);
      ++request.other_count;
    }
  }
  return request;
}

// decomp: BATTLE.PRG func_8007BE20 (F00238,
// src/BATTLE/BATTLE.PRG/146C.c:8581, 0x8007BE20-0x8007BEC4)
void StartScriptedActorEffect(std::uint16_t effect_id, std::int32_t target,
                              ScriptedEffectTriggerHost& host) {
  host.BeginActionEffect(ScriptedActorEffectRequest(effect_id, target, host),
                         kPrimaryRequestVariant, kPrimaryRequestArgument);
}

// decomp: BATTLE.PRG func_8007BEC4 (F00239,
// src/BATTLE/BATTLE.PRG/146C.c:8611, 0x8007BEC4-0x8007BEE4)
void ReleaseScriptedEffectTrigger(std::int32_t track,
                                  ScriptedEffectTriggerHost& host) {
  host.StartEffectTrack(track);
}

// decomp: BATTLE.PRG func_8007BEE4 (F00240,
// src/BATTLE/BATTLE.PRG/146C.c:8613, 0x8007BEE4-0x8007BF04)
std::int32_t ScriptedEffectPlaybackState(ScriptedEffectTriggerHost& host) {
  return host.PrimaryEffectPhase();
}

// decomp: BATTLE.PRG func_8007BF0C (F00242,
// src/BATTLE/BATTLE.PRG/146C.c:8617, 0x8007BF0C-0x8007BF2C)
bool IsScriptedEffectTriggerReady(ScriptedEffectTriggerHost& host) {
  return host.CanStartEffectTrack() != 0;
}
}  // namespace VS::Effects
