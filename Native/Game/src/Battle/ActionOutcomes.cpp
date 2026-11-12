// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionOutcomes.hpp"

#include <array>

#include "Save/Snapshot.hpp"

namespace VS::Battle {
namespace {
constexpr std::uint8_t kDynamicSpawn = 0x80;
constexpr std::uint8_t kDefeatedCommand = 0x20;
constexpr std::uint8_t kBlockSound = 0x33;
constexpr std::uint8_t kBlockImpact = 3;
constexpr std::uint8_t kAlternateBlockImpact = 4;
constexpr std::uint8_t kRecoveryImpact = 5;
constexpr std::array<std::array<std::uint8_t, 2>, 10> kWeaponSounds{{
    {0x47, 0x48},
    {0x49, 0x4A},
    {0x4B, 0x4C},
    {0x50, 0x51},
    {0x52, 0x53},
    {0x4D, 0},
    {0x4E, 0x4F},
    {0x54, 0x55},
    {0x69, 0x6A},
    {0x5B, 0x5C},
}};
void PlayHitSound(std::uint8_t impact, int source_actor, ActionPoint point,
                  ActionOutcomeWorld& world) {
  if (impact == kBlockImpact) {
    world.PlaySound(kBlockSound, point);
  } else if (impact <= 2 || impact == kAlternateBlockImpact) {
    const auto category = world.WeaponCategory(source_actor);
    if (category == 0 || category > kWeaponSounds.size()) return;
    const auto sounds = kWeaponSounds.at(category - 1);
    world.PlaySound(sounds.at(0), point);
    if (sounds.at(1) != 0) world.PlaySound(sounds.at(1), point);
  }
}
}  // namespace

// decomp: _enemyDefeated (src/BATTLE/BATTLE.PRG/146C.c:1824).
void ResolveEnemyDefeat(int source_actor, int count_kill,
                        EnemyDefeatRecord& enemy) {
  if (enemy.Spawn() == kDynamicSpawn || enemy.Defeated()) return;
  if (count_kill != 0 && source_actor == 0) {
    enemy.RecordKill();
    enemy.UpdateWeaponStreak();
  }
  enemy.QueueLoot();
  enemy.MarkDefeated();
}

// decomp: func_8006C5AC (src/BATTLE/BATTLE.PRG/146C.c:1955).
int PlayWeaponHitSounds(int source_actor, ActionOutcomeWorld& world) {
  auto& action = world.Queued();
  for (std::size_t index = 0; index < action.Count(); ++index) {
    const auto target = action.Target(index);
    if (target.targets_position) continue;
    const auto point = world.Placement(target.target_actor);
    PlayHitSound(action.Target(index).impact_kind, source_actor, point, world);
  }
  return 1;
}

// decomp: func_8006CA20 (src/BATTLE/BATTLE.PRG/146C.c:2106).
void ApplyActionSourceEffects(int source_actor, ActionOutcomeWorld& world) {
  if (source_actor >= kActionActorSlots) return;
  auto& action = world.Queued();
  const auto primary = action.Primary();
  if (primary.target_actor != source_actor || primary.targets_position) return;
  if (primary.impact_kind <= 1) {
    action.DamageReaction(source_actor);
  } else if (primary.impact_kind == kRecoveryImpact && source_actor == 0) {
    world.RecoveryPose();
  }
  action.ApplyPrimaryResult();
}

// decomp: func_80074A20 (src/BATTLE/BATTLE.PRG/146C.c:5315).
int BeginActorDefeatAnimation(int actor, ActorDefeatWorld& world) {
  world.RefreshReservations(actor);
  if (!world.HasActor(actor)) return 0;
  world.SetCommandState(actor, 0);
  if (world.Hp(actor) != 0) return 0;
  world.HideMarker(actor);
  world.SetCommandState(actor, kDefeatedCommand);
  if (world.Retained(actor)) return 0;
  world.BeginDeath(actor);
  world.LockAnimation(actor);
  return world.Spawn(actor) != kDynamicSpawn || actor == 0 ? 1 : 0;
}

// decomp: func_800F9818 (src/BATTLE/INITBTL.PRG/18.c:240).
void ApplyLearnedActionUnlocks(LearnedActionWorld& world) {
  constexpr std::uint8_t kFirstBit = 0x80;
  for (std::size_t index = 0; index < Save::kLearnedActionCount; ++index) {
    const auto learned =
        world.LearnedByte(index / Save::kActionsPerLearnedByte);
    const auto mask = kFirstBit >> (index % Save::kActionsPerLearnedByte);
    world.SetUnlocked(index, (learned & mask) != 0);
  }
}
}  // namespace VS::Battle
