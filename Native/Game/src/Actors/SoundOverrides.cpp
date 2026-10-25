#include "Actors/SoundOverrides.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>

#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "State/Timers.h"
}

// func_800AEEC4, src/BATTLE/BATTLE.PRG/44F14.c:886; MIPS 800AEEC4-800AEF90.
void Audio_UpdateActorSoundTransition(Actors_ActorRecord* actor) {
  if (actor == nullptr) std::abort();
  auto& current = actor->soundEffect;
  const auto& target = actor->soundEffectTarget;
  const int remaining = current.parameter;
  if (current.kind != 1 || remaining == 0) return;

  const int step = std::min(State_tickSpeed / 2, remaining);
  current.flags = static_cast<std::uint8_t>(
      current.flags + (target.flags - current.flags) * step / remaining);
  current.level = static_cast<std::uint8_t>(
      current.level + (target.level - current.level) * step / remaining);
  current.parameter = static_cast<std::uint8_t>(remaining - step);
}
