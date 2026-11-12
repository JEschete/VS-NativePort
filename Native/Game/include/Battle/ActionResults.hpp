#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Battle/ActorVitals.hpp"

// Applies the current action's results to the actor it reached and picks the
// actor's reaction. The action's hits and the reactions are reached through
// ActionTargetEffects, so the selection reads no action queue, actor table or
// PS1 address.
namespace VS::Battle {
// Actor ids below this are battle actor slots; larger ids name world targets.
inline constexpr int kActionActorSlots = 16;

// The identifying fields of one hit in the current action.
struct ActionTargetHit {
  bool targets_position = false;
  int target_actor = 0;
  std::uint8_t impact_kind = 0;
};

class ActionTargetEffects {
 public:
  ActionTargetEffects() = default;
  ActionTargetEffects(const ActionTargetEffects&) = delete;
  ActionTargetEffects& operator=(const ActionTargetEffects&) = delete;
  ActionTargetEffects(ActionTargetEffects&&) = delete;
  ActionTargetEffects& operator=(ActionTargetEffects&&) = delete;
  virtual ~ActionTargetEffects() = default;

  // Queues a world-locked label at the impact position (func_80093B04).
  virtual void QueueWorldLabel() = 0;
  // The current action's hit count, read before each hit.
  [[nodiscard]] virtual std::size_t HitCount() = 0;
  [[nodiscard]] virtual ActionTargetHit Hit(std::size_t index) = 0;
  // Whether the current action's category targets an area.
  [[nodiscard]] virtual bool IsAreaAction() = 0;
  // Applies one matching hit to its actor (_applyBattleAbilityEffect).
  virtual void ApplyHit(std::size_t index) = 0;
  // Plays the damage reaction toward the action origin (func_8009EA14).
  virtual void PlayDamageReaction(int actor_id) = 0;
  [[nodiscard]] virtual bool HasActorRecord(int actor_id) = 0;
  // Plays the defense animation toward the action origin (func_8009DDF4).
  virtual void PlayDefense(int actor_id) = 0;
  // Ashley's recovery pose.
  virtual void PlayRecoveryPose() = 0;
};

// Applies every hit aimed at `actor_id` and plays the strongest reaction
// (func_8006CB04). World target ids queue a label when tagged and apply no
// hit. Requires a nonnegative `actor_id`.
void ApplyActionTargetEffects(int actor_id, ActionTargetEffects& effects);

enum class ActionResultField : std::size_t {
  kHp,
  kMp,
  kRisk,
  kStrength,
  kIntelligence,
  kAgility,
  kMaxHp,
  kMaxMp,
  kWeaponDp,
  kWeaponPp,
  kArmorDp,
  kShieldPp,
  kCount
};
struct ActionResultChange {
  std::int16_t amount{};
  std::uint8_t mode{};
  friend bool operator==(const ActionResultChange&,
                         const ActionResultChange&) = default;
};
struct ActionResultLimb {
  std::int16_t hp{};
  std::int16_t armor_dp{};
  friend bool operator==(const ActionResultLimb&,
                         const ActionResultLimb&) = default;
};
struct ActorActionResult {
  std::uint8_t target{};
  std::int8_t limb{};
  std::uint8_t impact{};
  std::array<ActionResultChange,
             static_cast<std::size_t>(ActionResultField::kCount)>
      changes{};
  std::array<ActionResultLimb, Items::kLimbCount> limbs{};
  // These names retain the original hit record's reversed semantics.
  std::uint32_t inflict_statuses{};
  std::uint32_t remove_statuses{};
  friend bool operator==(const ActorActionResult&,
                         const ActorActionResult&) = default;
};

class ActorActionResultWorld {
 public:
  virtual ~ActorActionResultWorld() = default;
  virtual void CommitEquipmentStats() = 0;
  virtual ActorActionResult ReadHit() = 0;
  virtual ActorVitalsAccess* TargetStats(std::uint8_t slot) = 0;
  virtual void Number(int slot, int limb, std::uint32_t amount, int label) = 0;
  virtual void StatusLabel(int slot, std::uint32_t mask, bool removing) = 0;
  virtual void RemoveStatuses(ActorVitalsAccess& stats, std::uint32_t mask) = 0;
  virtual void InflictStatuses(ActorVitalsAccess& stats,
                               std::uint32_t mask) = 0;
};

// Callbacks observe committed writes; the target slot/stats owner are cached,
// while hit fields and stats are reread after callbacks. False refuses missing
// stats or the selected armor limb, only when that limb is actually used.
// decomp: _applyBattleAbilityEffect (src/BATTLE/BATTLE.PRG/146C.c:14018).
bool ApplyActorActionResult(ActorActionResultWorld& world);
}  // namespace VS::Battle
