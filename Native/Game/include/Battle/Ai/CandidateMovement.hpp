#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Battle/Ai/MovementBehavior.hpp"
#include "Battle/Ai/PlanningPrimitives.hpp"

namespace VS::Battle::Ai {
struct CandidateMover {
  std::uint8_t gait = 0;                                         // 0x01
  std::uint8_t fleeing = 0;                                      // 0x0E
  std::uint8_t guarding = 0;                                     // 0x0F
  std::int8_t linked_actor = kCandidateTargetSlots;              // 0x16
  std::uint8_t alerted = 0;                                      // 0x18
  std::uint8_t guard_delay = 0;                                  // 0x44
  Core::Math::Position3 position{};                              // 0x4C
  std::int16_t point_height = 0;                                 // 0x82
  std::uint8_t actor_id = 0;                                     // 0x88
  std::uint8_t max_range = 0;                                    // 0xB3
  std::uint8_t range_mode = 0;                                   // 0xE9
  std::uint8_t plan_mode = 0;                                    // 0x12E
  std::uint8_t action_status = 0;                                // 0x131
  std::uint32_t planned_tile = 0;                                // 0x134
  std::uint8_t movement_target = 0;                              // 0x15C
  std::uint16_t movement_flags = 0;                              // 0x15E
  std::uint32_t destination = 0;                                 // 0x168
  std::array<std::uint32_t, kCandidateTargetSlots> relations{};  // 0x1A4
  std::array<std::uint32_t, kCandidateTargetSlots>
      distances_squared{};           // 0x1E4
  std::int32_t candidate_count = 0;  // 0x470
};

struct PursuitTarget {
  Core::Math::Position3 position{};
  std::uint32_t tile = 0;
};

// Candidate handles retain identity across calls. Calls taking the mover
// publish its writes before running and read back any changes they make.
class CandidateMovementWorld {
 public:
  virtual ~CandidateMovementWorld() = default;
  [[nodiscard]] virtual std::optional<std::size_t> RankedHandle(
      std::size_t rank) const = 0;
  [[nodiscard]] virtual std::optional<ActionCandidate> Candidate(
      std::size_t handle) const = 0;
  [[nodiscard]] virtual bool HasFrameActor(int target) const = 0;
  [[nodiscard]] virtual std::optional<PursuitTarget> TargetModel(
      int target) const = 0;
  [[nodiscard]] virtual std::optional<std::int16_t> TargetHeight(
      int target) const = 0;
  [[nodiscard]] virtual std::optional<std::uint32_t> FrameDistance(
      int observer, int target) const = 0;
  [[nodiscard]] virtual std::optional<ActorRoster> BattleActors() const = 0;
  [[nodiscard]] virtual int GroundHeight(int x, int z) const = 0;
  [[nodiscard]] virtual std::uint16_t ReadinessLevel() const = 0;
  virtual void CountApproachMove() = 0;
  [[nodiscard]] virtual bool CanApproach(CandidateMover& mover,
                                         std::size_t candidate) = 0;
  [[nodiscard]] virtual bool CooldownReady(CandidateMover& mover,
                                           std::size_t candidate) = 0;
  virtual void TryEngage(CandidateMover& mover) = 0;
  virtual void Disengage(CandidateMover& mover) = 0;
  virtual int Approach(CandidateMover& mover,
                       const ApproachRequest& request) = 0;
  virtual int Track(CandidateMover& mover, int target) = 0;
  virtual int Flee(CandidateMover& mover, int kind) = 0;
  virtual void Wander(CandidateMover& mover, std::uint32_t tile,
                      int radius) = 0;
  virtual void MoveToTile(CandidateMover& mover, std::uint32_t tile,
                          int radius) = 0;
  virtual RelatedActor FindRelated(CandidateMover& mover, int relation,
                                   bool want_tile) = 0;
  virtual void ReleaseReservations(CandidateMover& mover) = 0;
  virtual void Recover(CandidateMover& mover) = 0;
  virtual void ProjectRadial(CandidateMover& mover, std::uint32_t tile,
                             int angle, std::uint32_t distance) = 0;
  virtual void DispatchLocomotion(CandidateMover& mover, int kind,
                                  int angle) = 0;
  [[nodiscard]] virtual int RandomMasked(int mask) = 0;
  virtual void SetAwareness(CandidateMover& mover, int marker, int frames) = 0;
};

// Nothing denotes a refused missing record or invalid index, with a
// diagnostic. Integer results otherwise preserve the original return value.
// decomp: func_800D9E18 (src/BATTLE/BATTLE.PRG/71618.c:511).
std::optional<int> MoveForActionCandidate(CandidateMovementWorld& world,
                                          CandidateMover& mover,
                                          std::size_t candidate);
// decomp: func_800DA1D4 (src/BATTLE/BATTLE.PRG/71618.c:610).
std::optional<int> UpdateCombatPursuit(CandidateMovementWorld& world,
                                       CandidateMover& mover);
// decomp: func_800DA4BC (src/BATTLE/BATTLE.PRG/71618.c:788).
std::optional<int> TryApproachPlannedTarget(CandidateMovementWorld& world,
                                            CandidateMover& mover,
                                            std::size_t candidate);
// decomp: func_800DA5CC (src/BATTLE/BATTLE.PRG/71618.c:814).
std::optional<int> UpdateCloseFollowBehavior(CandidateMovementWorld& world,
                                             CandidateMover& mover);
// decomp: func_800DA6A4 (src/BATTLE/BATTLE.PRG/71618.c:839).
std::optional<int> UpdateEnemyPursuitBehavior(CandidateMovementWorld& world,
                                              CandidateMover& mover);
// decomp: func_800DA738 (src/BATTLE/BATTLE.PRG/71618.c:854).
std::optional<int> UpdateGuardMovementBehavior(CandidateMovementWorld& world,
                                               CandidateMover& mover);
}  // namespace VS::Battle::Ai
