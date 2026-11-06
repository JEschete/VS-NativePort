// Hand-maintained (tools/port/owned.csv).
#include "Battle/BattleMode.hpp"  // NOLINT(build/include): typed API

namespace VS::Battle {
namespace {
constexpr int kWeaponDrawnBit = 1;
// The pose transition SetActorBattleMode asks for: a toggle snaps, an
// explicit player mode animates.
constexpr int kSnapTransition = 0;
constexpr int kAnimatedTransition = 1;
// Enemy kinds a battle mode switches between.
constexpr int kBattleReadyKind = 4;
constexpr int kCalmKind = 8;
constexpr int kPlayerSlot = 0;
}  // namespace

// decomp: BATTLE.PRG func_8007C088 (src/BATTLE/BATTLE.PRG/146C.c:8655)
bool IsActorWeaponDrawn(int actor_slot, const BattleModeHost& host) {
  return (host.ActorWeaponDrawn(actor_slot) & kWeaponDrawnBit) != 0;
}

// decomp: BATTLE.PRG func_8007C0AC (src/BATTLE/BATTLE.PRG/146C.c:8657)
void SetActorBattleMode(int actor_slot, int battle_mode, BattleModeHost& host) {
  if (battle_mode == kToggleWeapon) {
    const int toggled = host.ActorWeaponDrawn(actor_slot) ^ kWeaponDrawnBit;
    host.SetActorWeaponDrawn(actor_slot, toggled);
    host.ShowWeaponDrawn(actor_slot, toggled & kWeaponDrawnBit,
                         kSnapTransition);
  } else if (actor_slot != kPlayerSlot) {
    const int kind = host.ActorKind(actor_slot);
    if (kind == kBattleReadyKind || kind == kCalmKind) {
      host.SetActorKind(actor_slot, battle_mode != kSheatheWeapon
                                        ? kBattleReadyKind
                                        : kCalmKind);
    }
  } else {
    host.SetActorWeaponDrawn(
        kPlayerSlot,
        (host.ActorWeaponDrawn(kPlayerSlot) & ~kWeaponDrawnBit) | battle_mode);
    host.ShowWeaponDrawn(kPlayerSlot, battle_mode, kAnimatedTransition);
  }
  if (actor_slot == kPlayerSlot) {
    host.SetHudBattleMode(host.CharacterWeaponDrawn() & kWeaponDrawnBit);
  }
}
}  // namespace VS::Battle
