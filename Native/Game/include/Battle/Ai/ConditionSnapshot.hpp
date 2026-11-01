#pragma once

#include <cstdint>
#include <optional>
#include <span>

// The per-actor condition table the action planner scores candidates against.
// The battle actor list and the planner's rows stay with their owners; the
// snapshot only sees an explicit list of readings and an output table, so it
// reaches no global actor list, scratchpad or PS1 address.
namespace VS::Battle::Ai {
inline constexpr int kBattleActorSlotCount = 16;

// An actor's health, magic, status bits and equipment durability.
struct ActorCondition {
  std::int16_t hp = 0;
  std::int16_t max_hp = 0;
  std::int16_t mp = 0;
  std::int16_t max_mp = 0;
  std::uint32_t statuses = 0;
  std::int16_t weapon_dp = 0;
  std::int16_t weapon_max_dp = 0;
  std::int16_t shield_dp = 0;
  std::int16_t shield_max_dp = 0;

  friend bool operator==(const ActorCondition&,
                         const ActorCondition&) = default;
};

// One battle actor as the snapshot reads it; an actor without stats has no
// condition.
struct ActorConditionReading {
  int actor_id = 0;
  std::optional<ActorCondition> condition;
};

// The rows a snapshot writes, one per actor slot.
class ActorConditionTable {
 public:
  virtual ~ActorConditionTable() = default;
  // Stores an actor's condition in its row; returns false when the actor ID
  // has no row.
  virtual bool StoreActorCondition(int actor_id,
                                   const ActorCondition& condition) = 0;
};

// Writes every reading's condition to its actor's row and leaves other rows
// alone (func_800DC424). An actor ID outside the slot range, an actor without
// stats or a rejected row aborts.
void CaptureActorConditionTable(std::span<const ActorConditionReading> readings,
                                ActorConditionTable& table);
}  // namespace VS::Battle::Ai
