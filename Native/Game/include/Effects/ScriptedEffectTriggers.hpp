#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>

#include "Effects/ActionEffectCallbacks.hpp"

// The scripted actor effect trigger lifecycle (BATTLE.PRG 146C.c:8581-8617):
// the event script starts a primary effect on one actor with every other
// loaded actor included, releases the effect's waiting trigger track, polls
// the primary effect's phase and asks whether a trigger track can start. The
// rules build the request; the host owns the actor table and the effect
// runtime.
namespace VS::Effects {
// VS::Battle::actors slots the request scans for the other actors.
inline constexpr std::size_t kScriptedEffectActorSlots = 16;

// The services the trigger lifecycle uses.
class ScriptedEffectTriggerHost : public PrimaryEffectSink {
 public:
  // VS::Battle::actors[slot] != NULL, for slot < kScriptedEffectActorSlots.
  [[nodiscard]] virtual bool ActorLoaded(std::size_t slot) = 0;
  // func_800CEF74.
  virtual void StartEffectTrack(std::int32_t track) = 0;
  // func_800CF48C.
  [[nodiscard]] virtual std::int32_t PrimaryEffectPhase() = 0;
  // func_800CEEBC.
  [[nodiscard]] virtual std::int32_t CanStartEffectTrack() = 0;
};

// The request func_8007BE20 begins: `effect_id` from the origin to part 0
// of actor `target` (its low byte), with every loaded actor in slots 0-15
// other than `target` (compared as a full word) as the others, in slot
// order.
[[nodiscard]] PrimaryEffectRequest ScriptedActorEffectRequest(
    std::uint16_t effect_id, std::int32_t target,
    ScriptedEffectTriggerHost& host);

// decomp: BATTLE.PRG func_8007BE20 (F00238,
// src/BATTLE/BATTLE.PRG/146C.c:8581, 0x8007BE20-0x8007BEC4)
void StartScriptedActorEffect(std::uint16_t effect_id, std::int32_t target,
                              ScriptedEffectTriggerHost& host);

// decomp: BATTLE.PRG func_8007BEC4 (F00239,
// src/BATTLE/BATTLE.PRG/146C.c:8611, 0x8007BEC4-0x8007BEE4)
void ReleaseScriptedEffectTrigger(std::int32_t track,
                                  ScriptedEffectTriggerHost& host);

// decomp: BATTLE.PRG func_8007BEE4 (F00240,
// src/BATTLE/BATTLE.PRG/146C.c:8613, 0x8007BEE4-0x8007BF04)
[[nodiscard]] std::int32_t ScriptedEffectPlaybackState(
    ScriptedEffectTriggerHost& host);

// decomp: BATTLE.PRG func_8007BF0C (F00242,
// src/BATTLE/BATTLE.PRG/146C.c:8617, 0x8007BF0C-0x8007BF2C)
[[nodiscard]] bool IsScriptedEffectTriggerReady(
    ScriptedEffectTriggerHost& host);
}  // namespace VS::Effects
