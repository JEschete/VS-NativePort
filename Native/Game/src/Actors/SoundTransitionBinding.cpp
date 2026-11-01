// Hand-maintained (tools/port/owned.csv).
#include "Actors/SoundTransitionBinding.hpp"

#include "Actors/SoundTransition.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ModelInstance.h"
#include "State/Timers.h"
}

namespace VS::Actors {
namespace {
SoundTransition FromSlot(const Actors_ModelEffectSlot& slot) {
  return {slot.kind, slot.level, slot.flags, slot.parameter};
}
}  // namespace

void UpdateActorSoundTransition(Actors_ActorRecord& actor) {
  const SoundTransition next =
      StepSoundTransition(FromSlot(actor.soundEffect),
                          FromSlot(actor.soundEffectTarget), State_tickSpeed);
  actor.soundEffect.kind = next.mode;
  actor.soundEffect.level = next.pan;
  actor.soundEffect.flags = next.volume;
  actor.soundEffect.parameter = next.frames_remaining;
}
}  // namespace VS::Actors
