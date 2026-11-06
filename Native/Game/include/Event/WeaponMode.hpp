#pragma once
// Hand-maintained (tools/port/owned.csv).
#include <cstdint>

#include "Event/ScriptStep.hpp"

namespace VS::Event {

// Operands of the script opcode that draws or sheathes Ashley's weapon.
struct WeaponModeCommand {
  std::uint8_t mode = 0;        // index into the weapon draw-state table
  std::uint8_t transition = 0;  // 0 = snap; otherwise animation variant + 1
};

// The world the opcode reads and changes: the event VM's wait/skip state,
// Ashley's battle mode, her animation and her weapon visibility.
class WeaponModeHost {
 public:
  WeaponModeHost() = default;
  WeaponModeHost(const WeaponModeHost&) = delete;
  WeaponModeHost& operator=(const WeaponModeHost&) = delete;
  WeaponModeHost(WeaponModeHost&&) = delete;
  WeaponModeHost& operator=(WeaponModeHost&&) = delete;
  virtual ~WeaponModeHost() = default;

  [[nodiscard]] virtual bool TransitionPending() const = 0;
  virtual void SetTransitionPending(bool pending) = 0;
  [[nodiscard]] virtual bool SkipActive() const = 0;
  [[nodiscard]] virtual bool WeaponModeGateOpen() const = 0;
  virtual void SetBattleMode(int mode) = 0;
  virtual void PlayAnimation(int animation) = 0;
  virtual void PlayDrawSound(int sheathe) = 0;
  [[nodiscard]] virtual bool AnimationHeld() const = 0;
  virtual void SnapVisibility(bool drawn) = 0;
};

inline constexpr int kTransitionAnimationBase = 109;
inline constexpr int kDrawnIdleAnimation = 101;
inline constexpr int kSheathedIdleAnimation = 6;

// decomp: func_800B9DB4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2158).
ScriptStep ScriptSetAshleyBattleMode(WeaponModeCommand command,
                                     WeaponModeHost& host);

}  // namespace VS::Event
