#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Battle/Ai/PlanningPrimitives.hpp"
#include "Core/Math/Distance.hpp"

namespace VS::Battle::Ai {
// Zero is the end of the list. Handles identify nodes, not actor slots;
// callbacks may change a node's ID or successor.
class DecisionActorList {
 public:
  virtual ~DecisionActorList() = default;
  [[nodiscard]] virtual std::size_t ActorHead() const = 0;
  [[nodiscard]] virtual std::optional<int> ActorId(std::size_t node) const = 0;
  [[nodiscard]] virtual std::size_t ActorNext(std::size_t node) const = 0;
};

class ActionExecutionWorld {
 public:
  virtual ~ActionExecutionWorld() = default;
  [[nodiscard]] virtual int CandidateCount() const = 0;
  [[nodiscard]] virtual std::optional<std::size_t> RankedCandidate(
      std::size_t rank) const = 0;
  [[nodiscard]] virtual std::size_t FallbackCandidate() const = 0;
  [[nodiscard]] virtual std::optional<ActionCandidate> Candidate(
      std::size_t handle) const = 0;
  virtual void StoreCandidate(std::size_t handle,
                              const ActionCandidate& candidate) = 0;
  virtual bool CanExecute(std::size_t handle) = 0;
  virtual void TryEngage() = 0;
  virtual bool EngagementReady() = 0;
  virtual void SelectCandidate(std::size_t handle) = 0;
  [[nodiscard]] virtual bool TargetMoving(unsigned target) const = 0;
  virtual int RandomMasked(int mask) = 0;
  virtual void IssueCommand(int command) = 0;
  virtual bool RequiresCooldown(std::size_t handle) = 0;
  [[nodiscard]] virtual bool UsesRoomDelay() const = 0;
  [[nodiscard]] virtual std::optional<std::uint8_t> RoomDelay() const = 0;
  virtual void SetDelay(std::uint8_t delay) = 0;
  [[nodiscard]] virtual bool HasRoom() const = 0;
  [[nodiscard]] virtual std::optional<int> SpecialActor(
      unsigned slot) const = 0;
  [[nodiscard]] virtual std::optional<bool> GimmickReady(int actor) const = 0;
};

struct DecisionDispatchState {
  bool restricted = false;
  bool partial = false;
  unsigned tile_x = 0;
  unsigned tile_z = 0;
  Core::Math::Position3 position{};
  std::int16_t point_height = 0;
};

struct DecisionPlayer {
  bool has_ai = false;
  bool moving = false;
  std::int16_t y = 0;
  std::int16_t point_height = 0;
  std::int16_t body_height = 0;
};

class DecisionUpdateWorld : public DecisionActorList {
 public:
  virtual bool SelectActor(int id) = 0;
  [[nodiscard]] virtual DecisionDispatchState State() const = 0;
  virtual int ElapsedTicks() = 0;
  [[nodiscard]] virtual int TickBudget() const = 0;
  virtual void CacheTick(int tick) = 0;
  virtual void IssueCommand(int command) = 0;
  [[nodiscard]] virtual std::optional<DecisionPlayer> Player() const = 0;
  [[nodiscard]] virtual std::optional<Core::Math::Position3>
  PlayerBattlePosition() const = 0;
  virtual void StorePlayerDistance(std::uint32_t distance) = 0;
  virtual std::optional<int> ExecutePlannedActions() = 0;
  [[nodiscard]] virtual std::optional<std::uint16_t> TileFlags(
      unsigned x, unsigned z) const = 0;
  virtual void ResetMovement(std::uint8_t area) = 0;
  virtual void ClearFirstDirectionWord() = 0;
  virtual void DispatchMovement() = 0;
  virtual void UpdateLocomotion() = 0;
};

// Missing values denote a diagnosed refusal, not an unsuccessful action.
// decomp: func_800DAC80 (src/BATTLE/BATTLE.PRG/71618.c:1006).
std::optional<int> TryExecuteActionCandidate(ActionExecutionWorld& world,
                                             std::size_t candidate);
// decomp: func_800DAD9C (src/BATTLE/BATTLE.PRG/71618.c:1093).
std::optional<int> TryExecutePlannedActions(ActionExecutionWorld& world);
// decomp: func_800DAED0 (src/BATTLE/BATTLE.PRG/71618.c:1128).
bool UpdateActorDecisions(DecisionUpdateWorld& world);
}  // namespace VS::Battle::Ai
