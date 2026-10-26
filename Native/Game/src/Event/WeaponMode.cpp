// Hand-maintained (tools/port/owned.csv).
#include "Event/WeaponMode.h"

#include <cstdint>
#include <cstdlib>
#include <iterator>

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Battle/Abilities.h"
#include "Battle/BattleMode.h"
#include "Battle/Damage.h"
#include "Event/Actors.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptVm.h"
#include "State/Flags.h"
}

namespace {
constexpr int kAshley = 0;
constexpr int kWeaponModeGate = 0xAC;
constexpr int kSkipActive = 2;
constexpr int kBlendFrames = 10;
constexpr int kTransitionAnimationBase = 109;
constexpr int kDrawnIdleAnimation = 101;
constexpr int kSheathedIdleAnimation = 6;
constexpr std::uint32_t kAnimationHeld = 0x80;
constexpr std::uint32_t kWeaponVisibility = 0x1800;
constexpr std::uint32_t kLowWord = 0xFFFF;

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Debugf("event", "%s", message);
    std::abort();
  }
}

void PlayAnimation(int animation) {
  Require(Actors_PlayActorAnimation(kAshley, animation, 0, kBlendFrames) >= 0,
          "Script weapon-mode animation failed");
}

void SnapVisibility(bool drawn) {
  Require(Actors_actorRecords[kAshley] != nullptr,
          "Script weapon visibility requires Ashley's model state");
  auto flags = Actors_ActorStateFlags(kAshley) & kLowWord;
  flags = drawn ? flags | kWeaponVisibility : flags & ~kWeaponVisibility;
  Actors_SetActorDisplayFlags(kAshley, static_cast<int>(flags));
  Actors_ResetActorWeaponVisibilityBlend(kAshley);
}
}  // namespace

// decomp: func_800B9DB4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2158;
// original nonmatchings/4A0A8/func_800B9DB4.s, 0x800B9DB4-0x800B9F24).
intptr_t Event_ScriptSetAshleyBattleMode(u_char* command, int16_t) {
  Require(command != nullptr, "Script weapon-mode command is missing");
  const int mode = command[1];
  if (Event_weaponTransitionPending == 0) {
    if (State_ScriptFlag(kWeaponModeGate) == 0) return 0;
    Require(static_cast<std::size_t>(mode) < std::size(Event_weaponDrawStates),
            "Script weapon-mode table index is out of bounds");
    Require(Battle_actors[kAshley] != nullptr &&
                Battle_characterState != nullptr &&
                Actors_actorRecords[kAshley] != nullptr,
            "Script weapon mode requires Ashley's battle and model state");
    Battle_SetActorBattleMode(kAshley, Event_weaponDrawStates[mode]);
    if (Event_skipState != kSkipActive && command[2] != 0) {
      PlayAnimation((command[2] - 1) * 2 + (mode ^ 1) +
                    kTransitionAnimationBase);
      Actors_PlayActorWeaponDrawSound(kAshley, mode ^ 1);
      Event_weaponTransitionPending = 1;
      return 1;
    }
  } else if (Event_skipState == kSkipActive) {
    Event_weaponTransitionPending = 0;
  } else {
    Require(Actors_actorRecords[kAshley] != nullptr,
            "Pending script weapon mode requires Ashley's model state");
    if ((Actors_ActorStateFlags(kAshley) & kAnimationHeld) == 0) return 1;
    PlayAnimation(mode != 0 ? kDrawnIdleAnimation : kSheathedIdleAnimation);
    Event_weaponTransitionPending = 0;
    return 0;
  }
  SnapVisibility(mode != 0);
  return 0;
}
