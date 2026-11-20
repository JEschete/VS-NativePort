// Hand-maintained (tools/port/owned.csv).
#include "Actors/SoundTransitionBinding.hpp"

#include "Actors/ActorRecord.hpp"
#include "Actors/ModelInstance.hpp"
#include "Actors/SoundTransition.hpp"
#include "Legacy/Psx.hpp"
#include "State/Timers.hpp"

namespace VS::Actors {
namespace {
SoundTransition FromSlot(const VS::Actors::ModelEffectSlot& slot) {
  return {slot.kind, slot.level, slot.flags, slot.parameter};
}
}  // namespace

void UpdateActorSoundTransition(VS::Actors::ActorRecord& actor) {
  const SoundTransition next = StepSoundTransition(
      FromSlot(actor.sound_effect), FromSlot(actor.sound_effect_target),
      VS::State::tickSpeed);
  actor.sound_effect.kind = next.mode;
  actor.sound_effect.level = next.pan;
  actor.sound_effect.flags = next.volume;
  actor.sound_effect.parameter = next.frames_remaining;
}
}  // namespace VS::Actors
