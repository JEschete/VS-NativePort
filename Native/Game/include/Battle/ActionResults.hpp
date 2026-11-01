#pragma once

#include <cstddef>
#include <cstdint>

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
}  // namespace VS::Battle
