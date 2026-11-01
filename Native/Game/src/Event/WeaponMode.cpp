// Hand-maintained (tools/port/owned.csv).
// Typed core of the weapon-mode script opcode; WeaponModeLive.cpp binds it to
// the event VM, Ashley's battle actor and her model record.
#include "Event/WeaponMode.hpp"

namespace VS::Event {

// decomp: func_800B9DB4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2158;
// original nonmatchings/4A0A8/func_800B9DB4.s, 0x800B9DB4-0x800B9F24).
ScriptStep ScriptSetAshleyBattleMode(WeaponModeCommand command,
                                     WeaponModeHost& host) {
  const int mode = command.mode;
  const int sheathe = mode ^ 1;
  if (!host.TransitionPending()) {
    if (!host.WeaponModeGateOpen()) return ScriptStep::kAdvance;
    host.SetBattleMode(mode);
    if (!host.SkipActive() && command.transition != 0) {
      host.PlayAnimation(((command.transition - 1) * 2) + sheathe +
                         kTransitionAnimationBase);
      host.PlayDrawSound(sheathe);
      host.SetTransitionPending(true);
      return ScriptStep::kHold;
    }
  } else if (host.SkipActive()) {
    host.SetTransitionPending(false);
  } else {
    if (!host.AnimationHeld()) return ScriptStep::kHold;
    host.PlayAnimation(mode != 0 ? kDrawnIdleAnimation
                                 : kSheathedIdleAnimation);
    host.SetTransitionPending(false);
    return ScriptStep::kAdvance;
  }
  host.SnapVisibility(mode != 0);
  return ScriptStep::kAdvance;
}

}  // namespace VS::Event
