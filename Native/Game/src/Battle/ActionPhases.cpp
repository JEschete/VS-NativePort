// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionPhases.hpp"

namespace VS::Battle {
// decomp: func_80069D14 (src/BATTLE/BATTLE.PRG/146C.c:761).
void ReleaseQueuedActorAttacks(ActionPhaseWorld& world) {
  constexpr std::uint8_t kAttackOpcode = 0x18;
  for (int slot = 0; slot < Actors::kCommandActorSlots; ++slot) {
    const auto command = world.Command(slot);
    if (command.has_value() &&
        command.value().state == Actors::kCommandQueued &&
        command.value().opcode == kAttackOpcode) {
      world.SetCommandState(slot, Actors::kCommandActionPreviewMissing);
    }
  }
}

// decomp: func_8006F5FC (src/BATTLE/BATTLE.PRG/146C.c:3228).
void WaitForPostActionMovement(ActionPhaseWorld& world) {
  world.SetControlMode(Field::FieldControlMode::kGimmickMotion);
  world.SetPhase(ActionPhase::kInitial);
  world.RestoreIdle(0);
}

// decomp: func_8006FBCC (src/BATTLE/BATTLE.PRG/146C.c:3388).
void BeginPostActionRecovery(std::int32_t reward_mode,
                             ActionPhaseWorld& world) {
  constexpr std::uint32_t kPresent = 1;
  for (int slot = 0; slot < Actors::kCommandActorSlots; ++slot) {
    if (slot != world.PrimaryActor() &&
        (world.ActorFlags(slot) & Actors::kStateActionAnimating) == 0 &&
        (world.ActorFlags(slot) & kPresent) != 0) {
      world.RestoreIdle(slot);
    }
  }
  world.RestoreRoomPalettes();
  world.FreezeAnimations();
  world.EnableClutAnimation();
  world.ResumeRoomAnimations();
  world.ShowActorMarkers();
  world.SetRecoveryReward(reward_mode);
  world.SetTickSpeed(2);
  world.SetPhase(ActionPhase::kRecovery);
}

// decomp: func_8006FCBC (src/BATTLE/BATTLE.PRG/146C.c:3408).
void BeginActionTrailFade(const ActionPhaseSource& source,
                          ActionPhaseWorld& world) {
  world.SetPhase(ActionPhase::kTrailFade);
  const int bone = world.TrailBone(source.Actor());
  world.FadeTrails(source.Actor(), bone);
}

// decomp: func_8006FD0C (src/BATTLE/BATTLE.PRG/146C.c:3418).
void FinishBattleAbilitySequence(ActionPhaseWorld& world) {
  world.SetPhase(ActionPhase::kAbilityFinished);
}

// decomp: func_80072B9C (src/BATTLE/BATTLE.PRG/146C.c:4414).
void ResetActionPhase(ActionPhaseWorld& world) {
  world.SetPhase(ActionPhase::kInitial);
}

// decomp: func_800734C0 (src/BATTLE/BATTLE.PRG/146C.c:4646).
void BeginAshleyWeaponToggle(ActionPhaseWorld& world) {
  for (int slot = 0; slot < Actors::kCommandActorSlots; ++slot) {
    world.RestoreIdle(slot);
  }
  world.ToggleAshleyWeapon();
  world.SetControlMode(Field::FieldControlMode::kWeaponToggle);
}
}  // namespace VS::Battle
