// Hand-maintained (tools/port/owned.csv).
// Binds VS::Event::ScriptSetAshleyBattleMode to the event VM opcode slot,
// Ashley's battle actor and her model record.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>

#include "Actors/ActorRecord.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/Movement.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/BattleModeLive.hpp"
#include "Battle/Damage.hpp"
#include "Event/Actors.hpp"
#include "Event/ScriptActors.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Event/WeaponMode.hpp"
#include "Event/WeaponModeOpcode.hpp"
#include "State/ScriptFlagsLive.hpp"

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
    VS::Core::Errorf("event", "%s", message);
    std::abort();
  }
}

class LiveWeaponModeHost final : public event::WeaponModeHost {
 public:
  [[nodiscard]] bool TransitionPending() const override {
    return event::weaponTransitionPending != 0;
  }
  void SetTransitionPending(bool pending) override {
    event::weaponTransitionPending = pending ? 1 : 0;
  }
  [[nodiscard]] bool SkipActive() const override {
    return event::skipState == kSkipActive;
  }
  [[nodiscard]] bool WeaponModeGateOpen() const override {
    return VS::State::LiveScriptFlag(kWeaponModeGate) != 0;
  }
  void SetBattleMode(int mode) override {
    Require(static_cast<std::size_t>(mode) < std::size(event::weaponDrawStates),
            "Script weapon-mode table index is out of bounds");
    Require(VS::Battle::actors[kAshley] != nullptr &&
                VS::Battle::characterState != nullptr &&
                VS::Actors::actorRecords[kAshley] != nullptr,
            "Script weapon mode requires Ashley's battle and model state");
    VS::Battle::LiveSetActorBattleMode(kAshley, event::weaponDrawStates[mode]);
  }
  void PlayAnimation(int animation) override {
    Require(VS::Actors::PlayLiveActorAnimation(kAshley, animation, 0,
                                               kBlendFrames) >= 0,
            "Script weapon-mode animation failed");
  }
  void PlayDrawSound(int sheathe) override {
    VS::Actors::PlayLiveActorWeaponDrawSound(kAshley, sheathe);
  }
  [[nodiscard]] bool AnimationHeld() const override {
    Require(VS::Actors::actorRecords[kAshley] != nullptr,
            "Pending script weapon mode requires Ashley's model state");
    return (VS::Actors::LiveActorStateFlags(kAshley) & kAnimationHeld) != 0;
  }
  void SnapVisibility(bool drawn) override {
    Require(VS::Actors::actorRecords[kAshley] != nullptr,
            "Script weapon visibility requires Ashley's model state");
    auto flags = VS::Actors::LiveActorStateFlags(kAshley) & kLowWord;
    flags = drawn ? flags | kWeaponVisibility : flags & ~kWeaponVisibility;
    VS::Actors::SetLiveActorDisplayFlags(kAshley, static_cast<int>(flags));
    VS::Actors::ResetLiveActorWeaponVisibilityBlend(kAshley);
  }
};
}  // namespace

// decomp: func_800B9DB4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2158): the opcode reads
// its mode at byte 1 and its transition variant at byte 2.
std::intptr_t VS::Event::ScriptSetAshleyBattleModeOpcode(std::uint8_t* command,
                                                         std::int16_t) {
  Require(command != nullptr, "Script weapon-mode command is missing");
  LiveWeaponModeHost host;
  const event::WeaponModeCommand decoded{.mode = command[1],
                                         .transition = command[2]};
  return static_cast<std::intptr_t>(
      event::ScriptSetAshleyBattleMode(decoded, host));
}
