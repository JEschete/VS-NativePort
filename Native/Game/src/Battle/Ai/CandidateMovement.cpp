// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/CandidateMovement.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <optional>

#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
extern "C" {
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
using Core::Math::Arctangent2;
using Core::Math::Cosine;
using Core::Math::Sine;
using Core::Math::SquareRootInteger;
constexpr int kTileUnits = 128;
constexpr int kTileCenter = 64;
constexpr int kRangeUnits = 32;
constexpr int kPursuitDistance = 640;
constexpr int kCloseFollowDistance = 288;
constexpr int kEscortFollowDistance = 384;
constexpr unsigned kCategoryShift = 20;
constexpr unsigned kCategoryMask = 127;
constexpr unsigned kOldRelationShift = 28;
constexpr int kEnemyDistance = 1024;
constexpr int kGuardDistance = 320;
constexpr int kGuardDelay = 10;
constexpr int kLostGuardAwareness = 6;
constexpr int kDestinationRadius = 256;
constexpr int kGuardCosineThreshold = -1200;
constexpr int kFixedShift = 12;
constexpr std::uint32_t kGuardNearSquared = 0x23FFF;
constexpr std::uint32_t kMinimumApproachSquared = 0x64000;
constexpr std::uint32_t kDestinationNearSquared = 0xFFFF;
constexpr std::uint32_t kUnboundedRange = 0x1F80;
constexpr std::uint16_t kIgnoreActionRange = 8;
constexpr std::uint32_t kActiveRelation = 1;
constexpr std::uint32_t kVisibleRelation = 1U << 5;
constexpr int kAlly = 2;
constexpr int kEnemy = 3;
constexpr unsigned kRelationShift = 6;
constexpr unsigned kRelationMask = 3;
constexpr int kApproachMode = 1;
constexpr int kDestinationMode = 2;
constexpr int kFleeMode = 4;
constexpr int kPlayerMode = 7;

std::nullopt_t Refuse(const char* message) {
  Core_Debugf("ai", "%s", message);
  return std::nullopt;
}

bool ValidTarget(int target) {
  return target >= 0 && target < kCandidateTargetSlots;
}

unsigned Classification(const CandidateMover& mover, int target) {
  return (mover.relations.at(target) >> kRelationShift) & kRelationMask;
}

int ApproachOrTrack(CandidateMovementWorld& world, CandidateMover& mover,
                    const ApproachRequest& request) {
  if (world.Approach(mover, request) != 0) return 1;
  return world.Track(mover, request.target) != 0 ? 1 : 0;
}

std::optional<ActionCandidate> ReadCandidate(
    const CandidateMovementWorld& world, std::size_t handle) {
  auto candidate = world.Candidate(handle);
  if (!candidate) {
    return Refuse("AI candidate movement requires a candidate record");
  }
  return candidate;
}

std::optional<std::size_t> FirstCandidate(const CandidateMovementWorld& world) {
  auto handle = world.RankedHandle(0);
  if (!handle) {
    return Refuse("AI candidate movement requires a ranked candidate");
  }
  return handle;
}

std::uint32_t RangeSquared(int distance, std::uint8_t range) {
  const auto limit = static_cast<std::uint32_t>(distance) +
                     static_cast<std::uint32_t>(range) * kRangeUnits;
  return limit * limit;
}

void CountAndEngage(CandidateMovementWorld& world, CandidateMover& mover,
                    const ActionCandidate& candidate, unsigned target,
                    std::uint32_t limit) {
  world.CountApproachMove();
  if (mover.distances_squared.at(target) < limit &&
      (mover.relations.at(candidate.TargetId()) & kActiveRelation) != 0) {
    world.TryEngage(mover);
  }
}

std::optional<int> NormalApproach(CandidateMovementWorld& world,
                                  CandidateMover& mover, std::size_t handle,
                                  unsigned target) {
  auto candidate = ReadCandidate(world, handle);
  if (!candidate) return std::nullopt;
  const auto limit =
      (mover.movement_flags & kIgnoreActionRange) != 0
          ? kUnboundedRange * kUnboundedRange
          : RangeSquared(SquareRootInteger(candidate.value().DistanceSquared()),
                         mover.max_range);
  const bool allied = Classification(mover, static_cast<int>(target)) == kAlly;
  const bool active = allied || world.CanApproach(mover, handle);
  if (active) {
    candidate = ReadCandidate(world, handle);
    if (!candidate) return std::nullopt;
  }
  const auto distance_squared =
      active ? std::bit_cast<std::uint32_t>(candidate.value().DistanceSquared())
             : std::max(limit, kMinimumApproachSquared);
  const int distance =
      SquareRootInteger(std::bit_cast<std::int32_t>(distance_squared));
  if (active) CountAndEngage(world, mover, candidate.value(), target, limit);
  return ApproachOrTrack(world, mover,
                         {.target = static_cast<int>(target),
                          .distance = distance,
                          .option = allied ? 0 : 1});
}

int TileCenter(unsigned component) {
  return static_cast<int>(component) * kTileUnits + kTileCenter;
}

std::optional<bool> DestinationTooClose(const CandidateMovementWorld& world,
                                        const CandidateMover& mover,
                                        const ActionCandidate& candidate,
                                        int target) {
  const auto model = world.TargetModel(target);
  const auto height = world.TargetHeight(target);
  if (!model || !height) {
    return Refuse("AI destination range requires a target model and AI height");
  }
  const int x = TileCenter(PackedTileX(mover.planned_tile));
  const int z = TileCenter(PackedTileZ(mover.planned_tile));
  const Core::Math::Position3 point{
      static_cast<std::int16_t>(x),
      static_cast<std::int16_t>(world.GroundHeight(x, z)),
      static_cast<std::int16_t>(z)};
  const int distance =
      SquareRootInteger(Core::Math::SquaredDistanceWithHeightOffsets(
          point, model.value().position, mover.point_height, height.value()));
  return distance < SquareRootInteger(candidate.DistanceSquared()) -
                        static_cast<int>(candidate.Range());
}

void MoveToDestination(CandidateMovementWorld& world, CandidateMover& mover,
                       unsigned target) {
  const int x = TileCenter(PackedTileX(mover.destination));
  const int z = TileCenter(PackedTileZ(mover.destination));
  const int y = world.GroundHeight(x, z);
  if (Core::Math::SquaredVectorLength(
          x - mover.position.x, y - mover.position.y, z - mover.position.z) <=
      kDestinationNearSquared) {
    mover.movement_target = static_cast<std::uint8_t>(target);
    world.Wander(mover, mover.planned_tile, 2);
  } else {
    mover.gait = 1;
    world.MoveToTile(mover, mover.planned_tile, kDestinationRadius);
  }
}

std::optional<int> PursueCandidates(CandidateMovementWorld& world,
                                    CandidateMover& mover) {
  const auto first = FirstCandidate(world);
  if (!first) return std::nullopt;
  const auto candidate = ReadCandidate(world, first.value());
  if (!candidate) return std::nullopt;
  const unsigned target = candidate.value().TargetId();
  const auto moved = MoveForActionCandidate(world, mover, first.value());
  if (!moved) return std::nullopt;
  if (moved.value() != 0) return 1;
  if (mover.candidate_count < 0 ||
      mover.candidate_count > static_cast<int>(kCandidateCapacity)) {
    return Refuse("AI pursuit candidate count is outside its ranking");
  }
  if (mover.candidate_count <= 1) return 0;
  const auto readiness = world.ReadinessLevel();
  for (int rank = 1; rank < mover.candidate_count; ++rank) {
    const auto handle = world.RankedHandle(rank);
    if (!handle) return Refuse("AI pursuit has a missing ranked candidate");
    const auto next = ReadCandidate(world, handle.value());
    if (!next) return std::nullopt;
    if (next.value().Threshold() < readiness &&
        next.value().TargetId() != target) {
      return MoveForActionCandidate(world, mover, handle.value());
    }
  }
  return 0;
}

int PursuePlayer(CandidateMovementWorld& world, CandidateMover& mover) {
  world.Disengage(mover);
  if (Classification(mover, 0) != kEnemy) return 0;
  return ApproachOrTrack(
      world, mover, {.target = 0, .distance = kPursuitDistance, .option = 1});
}

std::optional<int> PursueNearest(CandidateMovementWorld& world,
                                 CandidateMover& mover, int target) {
  if (PursuePlayer(world, mover) != 0) return 1;
  const auto roster = world.BattleActors();
  if (!roster || roster.value().count > roster.value().ids.size()) {
    return Refuse("AI pursuit requires a bounded battle actor list");
  }
  auto nearest = std::numeric_limits<std::uint32_t>::max();
  for (const int id : roster.value().Ids()) {
    if (!ValidTarget(id)) {
      return Refuse("AI pursuit battle actor ID is outside its relations");
    }
    if (Classification(mover, id) == kEnemy &&
        mover.distances_squared.at(id) < nearest) {
      nearest = mover.distances_squared.at(id);
      target = id;
    }
  }
  return target >= 0 ? ApproachOrTrack(world, mover,
                                       {.target = target,
                                        .distance = kPursuitDistance,
                                        .option = 1})
                     : 0;
}

std::optional<int> PursuePending(CandidateMovementWorld& world,
                                 CandidateMover& mover) {
  if (PursuePlayer(world, mover) != 0) return 1;
  const auto first = FirstCandidate(world);
  if (!first) return std::nullopt;
  const auto candidate = ReadCandidate(world, first.value());
  if (!candidate) return std::nullopt;
  const int target = static_cast<int>(candidate.value().TargetId());
  int distance = kPursuitDistance;
  int option = 1;
  if (Classification(mover, target) != kEnemy) {
    if (mover.candidate_count < 2) return 0;
    const auto second = world.RankedHandle(1);
    if (!second) return Refuse("AI pursuit requires its second candidate");
    const auto next = ReadCandidate(world, second.value());
    if (!next) return std::nullopt;
    distance = SquareRootInteger(next.value().DistanceSquared());
    option = 0;
  }
  if (ApproachOrTrack(
          world, mover,
          {.target = target, .distance = distance, .option = option}) != 0) {
    return 1;
  }
  // The fallthrough keeps the first candidate's target if no nearer enemy
  // is found, including when the battle list is empty.
  return PursueNearest(world, mover, target);
}

std::optional<bool> GuardRadial(CandidateMovementWorld& world,
                                CandidateMover& mover, int target,
                                std::uint32_t threat_tile) {
  const auto model = world.TargetModel(target);
  if (!model) return Refuse("AI guard movement requires the protected model");
  const auto& position = model.value().position;
  const int threat_angle =
      Arctangent2(TileCenter(PackedTileX(threat_tile)) - position.x,
                  TileCenter(PackedTileZ(threat_tile)) - position.z);
  const int guard_angle =
      Arctangent2(mover.position.x - position.x, mover.position.z - position.z);
  const int dot = (Cosine(threat_angle) * Cosine(guard_angle) +
                   Sine(threat_angle) * Sine(guard_angle)) >>
                  kFixedShift;
  if (mover.guard_delay != 0) --mover.guard_delay;
  if (dot < kGuardCosineThreshold && mover.guard_delay == 0) return false;
  const auto tile = model.value().tile;
  const int angle = Arctangent2(static_cast<int>(PackedTileX(threat_tile)) -
                                    static_cast<int>(PackedTileX(tile)),
                                static_cast<int>(PackedTileZ(threat_tile)) -
                                    static_cast<int>(PackedTileZ(tile)));
  world.ProjectRadial(mover, tile, angle, kGuardDistance);
  mover.gait = 1;
  world.DispatchLocomotion(mover, 0, 0);
  if (mover.guard_delay == 0) mover.guard_delay = kGuardDelay;
  return true;
}
}  // namespace

// decomp: func_800D9E18 (src/BATTLE/BATTLE.PRG/71618.c:511).
std::optional<int> MoveForActionCandidate(CandidateMovementWorld& world,
                                          CandidateMover& mover,
                                          std::size_t handle) {
  if (mover.candidate_count == 0) return 0;
  const auto candidate = ReadCandidate(world, handle);
  if (!candidate) return std::nullopt;
  const unsigned target = candidate.value().TargetId();
  if (!world.HasFrameActor(static_cast<int>(target))) return 0;
  switch (mover.plan_mode) {
    case kApproachMode:
      return NormalApproach(world, mover, handle, target);
    case kDestinationMode: {
      if (!world.CooldownReady(mover, handle)) {
        return NormalApproach(world, mover, handle, target);
      }
      const auto current = ReadCandidate(world, handle);
      if (!current) return std::nullopt;
      if (current.value().Option(CandidateOption::kMinimumRange)) {
        const auto close = DestinationTooClose(world, mover, current.value(),
                                               static_cast<int>(target));
        if (!close) return std::nullopt;
        if (close.value()) return NormalApproach(world, mover, handle, target);
      }
      MoveToDestination(world, mover, target);
      return 1;
    }
    case kFleeMode:
      return world.Flee(mover, 0);
    case kPlayerMode:
      world.Disengage(mover);
      if (Classification(mover, 0) == kEnemy) {
        return ApproachOrTrack(
            world, mover,
            {.target = 0, .distance = kPursuitDistance, .option = 1});
      }
      return 0;
    default:
      return 0;
  }
}

// decomp: func_800DA1D4 (src/BATTLE/BATTLE.PRG/71618.c:610).
std::optional<int> UpdateCombatPursuit(CandidateMovementWorld& world,
                                       CandidateMover& mover) {
  switch (mover.action_status) {
    case 0:
      return PursueCandidates(world, mover);
    case 1:
      return PursuePending(world, mover);
    case 2:
      return PursueNearest(world, mover, -1);
    default:
      return 0;
  }
}

// decomp: func_800DA4BC (src/BATTLE/BATTLE.PRG/71618.c:788).
std::optional<int> TryApproachPlannedTarget(CandidateMovementWorld& world,
                                            CandidateMover& mover,
                                            std::size_t handle) {
  if (!ReadCandidate(world, handle)) return std::nullopt;
  if (!world.CanApproach(mover, handle)) return 0;
  const auto candidate = ReadCandidate(world, handle);
  if (!candidate) return std::nullopt;
  const unsigned target = candidate.value().TargetId();
  const int distance = SquareRootInteger(candidate.value().DistanceSquared());
  CountAndEngage(world, mover, candidate.value(), target,
                 RangeSquared(distance, mover.max_range));
  return ApproachOrTrack(
      world, mover,
      {.target = static_cast<int>(target), .distance = distance, .option = 1});
}

namespace {
std::optional<int> FollowLinkedActor(CandidateMovementWorld& world,
                                     CandidateMover& mover, int distance,
                                     int option) {
  const auto first = world.RankedHandle(0);
  if (mover.action_status == 0) {
    if (!first) return Refuse("AI close follow requires its action candidate");
    const auto moved = TryApproachPlannedTarget(world, mover, first.value());
    if (!moved) return std::nullopt;
    if (moved.value() != 0) return 1;
  }
  // The original lb sign-extends a numeric actor ID, not a character.
  // NOLINTNEXTLINE(bugprone-signed-char-misuse)
  const int target = static_cast<int>(mover.linked_actor);
  if (target >= kCandidateTargetSlots) return 0;
  if (!ValidTarget(target)) return Refuse("AI close follow target is negative");
  if (world.Approach(
          mover, {.target = target, .distance = distance, .option = option}) !=
      0) {
    if (mover.range_mode == 1 && mover.plan_mode != kPlayerMode) {
      if (!first) {
        return Refuse("AI close follow requires its recovery candidate");
      }
      const auto candidate = ReadCandidate(world, first.value());
      if (!candidate) return std::nullopt;
      mover.movement_target =
          static_cast<std::uint8_t>(candidate.value().TargetId());
      world.ReleaseReservations(mover);
      world.Recover(mover);
    }
    return 1;
  }
  return world.Track(mover, target) != 0 ? 1 : 0;
}
}  // namespace

// decomp: func_800DA5CC (src/BATTLE/BATTLE.PRG/71618.c:814).
std::optional<int> UpdateCloseFollowBehavior(CandidateMovementWorld& world,
                                             CandidateMover& mover) {
  return FollowLinkedActor(world, mover, kCloseFollowDistance, 1);
}

// decomp: func_800DAAD8 (src/BATTLE/BATTLE.PRG/71618.c:958).
std::optional<int> UpdateEscortCombatBehavior(CandidateMovementWorld& world,
                                              CandidateMover& mover) {
  const auto followed =
      FollowLinkedActor(world, mover, kEscortFollowDistance, 0);
  if (!followed || followed.value() != 0) return followed;
  std::array<std::size_t, kCandidateTargetSlots> visited{};
  std::size_t count = 0;
  for (auto node = world.ActorHead(); node != 0; node = world.ActorNext(node)) {
    if (count == visited.size()) return Refuse("AI escort list is unbounded");
    for (std::size_t index = 0; index < count; ++index) {
      if (visited.at(index) == node) return Refuse("AI escort list is cyclic");
    }
    visited.at(count++) = node;
    auto id = world.ActorId(node);
    if (!id || !ValidTarget(id.value())) {
      return Refuse("AI escort actor ID is invalid");
    }
    auto& relation = mover.relations.at(id.value());
    if (Classification(mover, id.value()) == kEnemy ||
        ((relation >> kCategoryShift) & kCategoryMask) == 0) {
      continue;
    }
    relation |= kRelationMask << kRelationShift;
    const int fled = world.Flee(mover, 0);
    id = world.ActorId(node);
    if (!id || !ValidTarget(id.value())) {
      return Refuse("AI escort callback changed its actor ID");
    }
    auto& current = mover.relations.at(id.value());
    current =
        (current & ~(kRelationMask << kRelationShift)) |
        (((current >> kOldRelationShift) & kRelationMask) << kRelationShift);
    if (fled != 0) return 1;
  }
  return 0;
}

// decomp: func_800DA6A4 (src/BATTLE/BATTLE.PRG/71618.c:839).
std::optional<int> UpdateEnemyPursuitBehavior(CandidateMovementWorld& world,
                                              CandidateMover& mover) {
  if (mover.fleeing != 0) return world.Flee(mover, 1);
  const int target = world.FindRelated(mover, kEnemy, false).actor_id;
  if (target >= kCandidateTargetSlots) {
    return Refuse("AI enemy pursuit target is outside the actor slots");
  }
  if (target >= 0 && world.FindRelated(mover, kAlly, false).actor_id >= 0 &&
      world.Approach(
          mover, {.target = target, .distance = kEnemyDistance, .option = 1}) !=
          0) {
    return 1;
  }
  return UpdateCombatPursuit(world, mover);
}

// decomp: func_800DA738 (src/BATTLE/BATTLE.PRG/71618.c:854).
std::optional<int> UpdateGuardMovementBehavior(CandidateMovementWorld& world,
                                               CandidateMover& mover) {
  const auto threat = world.FindRelated(mover, kEnemy, true);
  // The original lb sign-extends a numeric actor ID, not a character.
  // NOLINTNEXTLINE(bugprone-signed-char-misuse)
  const int target = static_cast<int>(mover.linked_actor);
  const bool valid = threat.actor_id >= 0;
  if ((target != mover.actor_id && target != kCandidateTargetSlots &&
       !ValidTarget(target)) ||
      threat.actor_id >= kCandidateTargetSlots) {
    return Refuse("AI guard target is outside the actor slots");
  }
  const bool active =
      target != mover.actor_id && target != kCandidateTargetSlots &&
      world.HasFrameActor(target) && Classification(mover, target) == kAlly;
  const auto was_guarding = mover.guarding;
  mover.guarding = active ? 1 : 0;
  if (active) {
    if (valid && (mover.relations.at(target) & kVisibleRelation) != 0) {
      const auto radial = GuardRadial(world, mover, target, threat.tile);
      if (!radial) return std::nullopt;
      if (radial.value()) return 1;
    }
    if (valid) {
      const auto distance = world.FrameDistance(target, threat.actor_id);
      if (!distance) return Refuse("AI guard requires the protected AI state");
      if (distance.value() <= kGuardNearSquared) {
        mover.movement_target = static_cast<std::uint8_t>(target);
        world.Recover(mover);
        return 1;
      }
    }
    if (ApproachOrTrack(
            world, mover,
            {.target = target, .distance = kGuardDistance, .option = 1}) != 0) {
      return 1;
    }
  }
  if (valid) {
    if (was_guarding != 0) {
      if (static_cast<std::uint32_t>(world.RandomMasked(kTileUnits)) <
          kTileUnits) {
        mover.alerted = 1;
      }
      world.SetAwareness(mover, kLostGuardAwareness, 0);
    }
    if (world.Flee(mover, 1) != 0) return 1;
  }
  return world.Flee(mover, 1);
}
}  // namespace VS::Battle::Ai
