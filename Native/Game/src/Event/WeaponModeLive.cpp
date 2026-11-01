// Hand-maintained (tools/port/owned.csv).
// Binds VS::Event::ScriptSetAshleyBattleMode to the event VM opcode slot,
// Ashley's battle actor and her model record.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>

#include "Event/WeaponMode.hpp"
#include "Event/WeaponModeOpcode.h"

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
namespace event = VS::Event;

constexpr int kAshley = 0;
constexpr int kWeaponModeGate = 0xAC;
constexpr int kSkipActive = 2;
constexpr int kBlendFrames = 10;
constexpr std::uint32_t kAnimationHeld = 0x80;
constexpr std::uint32_t kWeaponVisibility = 0x1800;
constexpr std::uint32_t kLowWord = 0xFFFF;

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Debugf("event", "%s", message);
    std::abort();
  }
}

class LiveWeaponModeHost final : public event::WeaponModeHost {
 public:
  [[nodiscard]] bool TransitionPending() const override {
    return Event_weaponTransitionPending != 0;
  }
  void SetTransitionPending(bool pending) override {
    Event_weaponTransitionPending = pending ? 1 : 0;
  }
  [[nodiscard]] bool SkipActive() const override {
    return Event_skipState == kSkipActive;
  }
  [[nodiscard]] bool WeaponModeGateOpen() const override {
    return State_ScriptFlag(kWeaponModeGate) != 0;
  }
  void SetBattleMode(int mode) override {
    Require(static_cast<std::size_t>(mode) < std::size(Event_weaponDrawStates),
            "Script weapon-mode table index is out of bounds");
    Require(Battle_actors[kAshley] != nullptr &&
                Battle_characterState != nullptr &&
                Actors_actorRecords[kAshley] != nullptr,
            "Script weapon mode requires Ashley's battle and model state");
    Battle_SetActorBattleMode(kAshley, Event_weaponDrawStates[mode]);
  }
  void PlayAnimation(int animation) override {
    Require(Actors_PlayActorAnimation(kAshley, animation, 0, kBlendFrames) >= 0,
            "Script weapon-mode animation failed");
  }
  void PlayDrawSound(int sheathe) override {
    Actors_PlayActorWeaponDrawSound(kAshley, sheathe);
  }
  [[nodiscard]] bool AnimationHeld() const override {
    Require(Actors_actorRecords[kAshley] != nullptr,
            "Pending script weapon mode requires Ashley's model state");
    return (Actors_ActorStateFlags(kAshley) & kAnimationHeld) != 0;
  }
  void SnapVisibility(bool drawn) override {
    Require(Actors_actorRecords[kAshley] != nullptr,
            "Script weapon visibility requires Ashley's model state");
    auto flags = Actors_ActorStateFlags(kAshley) & kLowWord;
    flags = drawn ? flags | kWeaponVisibility : flags & ~kWeaponVisibility;
    Actors_SetActorDisplayFlags(kAshley, static_cast<int>(flags));
    Actors_ResetActorWeaponVisibilityBlend(kAshley);
  }
};
}  // namespace

// decomp: func_800B9DB4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2158): the opcode reads
// its mode at byte 1 and its transition variant at byte 2.
intptr_t Event_ScriptSetAshleyBattleModeOpcode(u_char* command, int16_t) {
  Require(command != nullptr, "Script weapon-mode command is missing");
  LiveWeaponModeHost host;
  const event::WeaponModeCommand decoded{.mode = command[1],
                                         .transition = command[2]};
  return static_cast<intptr_t>(event::ScriptSetAshleyBattleMode(decoded, host));
}
