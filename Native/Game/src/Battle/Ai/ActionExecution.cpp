// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/ActionExecution.hpp"

#include <array>
#include <cstdint>
#include <optional>

extern "C" {
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
constexpr int kAttackCommand = 15;
constexpr int kBudgetCommand = 17;
constexpr int kMovingTargetHeight = 217;
constexpr int kFixedDelay = 50;
constexpr int kDefaultPlayerHeight = -64;
constexpr unsigned kSpecialActors = 4;
constexpr unsigned kAreaMask = 15;

std::nullopt_t Refuse(const char* message) {
  Core_Debugf("ai", "%s", message);
  return std::nullopt;
}

bool RefuseUpdate(const char* message) {
  Core_Debugf("ai", "%s", message);
  return false;
}

std::optional<int> ExecuteFallback(ActionExecutionWorld& world) {
  const auto handle = world.FallbackCandidate();
  auto entry = world.Candidate(handle);
  if (!entry) return Refuse("AI fallback candidate is missing");
  if (entry.value().Action() == 0) return 0;
  for (unsigned slot = 0; slot < kSpecialActors; ++slot) {
    if (!world.HasRoom()) return Refuse("AI fallback requires a room");
    const auto actor = world.SpecialActor(slot);
    if (!actor) continue;
    entry = world.Candidate(handle);
    if (!entry) return Refuse("AI fallback candidate disappeared");
    entry.value().SetTargetId(static_cast<unsigned>(actor.value()));
    world.StoreCandidate(handle, entry.value());
    // The target field truncates, but the original table index does not.
    const auto ready = world.GimmickReady(actor.value());
    if (!ready) return Refuse("AI fallback gimmick is missing or out of range");
    if (ready.value()) {
      const auto result = TryExecuteActionCandidate(world, handle);
      if (!result || result.value() != 0) return result;
    }
  }
  return 0;
}

bool UpdatePartial(DecisionUpdateWorld& world,
                   const DecisionDispatchState& state) {
  const auto player = world.Player();
  const auto position = world.PlayerBattlePosition();
  if (!player || !position) {
    return RefuseUpdate("AI partial decision requires the player records");
  }
  const auto& info = player.value();
  const int height = !info.has_ai ? kDefaultPlayerHeight
                     : info.moving && state.position.y < info.y
                         ? -static_cast<int>(info.body_height)
                         : info.point_height;
  world.StorePlayerDistance(Core::Math::SquaredDistanceWithHeightOffsets(
      state.position, position.value(), state.point_height, height));
  return world.ExecutePlannedActions().has_value();
}

bool UpdateSelected(DecisionUpdateWorld& world) {
  auto state = world.State();
  if (state.restricted && !state.partial) return true;
  const int elapsed = world.ElapsedTicks();
  if (world.TickBudget() < elapsed) {
    world.IssueCommand(kBudgetCommand);
    return true;
  }
  world.CacheTick(elapsed);
  state = world.State();
  if (state.restricted && state.partial) return UpdatePartial(world, state);
  const auto flags = world.TileFlags(state.tile_x, state.tile_z);
  if (!flags) {
    return RefuseUpdate("AI decision tile is missing or out of range");
  }
  world.ResetMovement(static_cast<std::uint8_t>(flags.value() & kAreaMask));
  world.ClearFirstDirectionWord();
  world.DispatchMovement();
  world.UpdateLocomotion();
  return true;
}
}  // namespace

// decomp: func_800DAC80 (src/BATTLE/BATTLE.PRG/71618.c:1006).
std::optional<int> TryExecuteActionCandidate(ActionExecutionWorld& world,
                                             std::size_t handle) {
  if (!world.Candidate(handle)) {
    return Refuse("AI execution candidate is missing");
  }
  if (!world.CanExecute(handle)) return 0;
  world.TryEngage();
  if (!world.EngagementReady()) return 0;
  world.SelectCandidate(handle);
  auto candidate = world.Candidate(handle);
  if (!candidate) return Refuse("AI execution candidate disappeared");
  if (world.TargetMoving(candidate.value().TargetId())) {
    const int limb = world.RandomMasked(2);
    candidate = world.Candidate(handle);
    if (!candidate) return Refuse("AI random callback removed the candidate");
    candidate.value().SetTargetLimb(static_cast<unsigned>(limb));
    candidate.value().SetHeightOffset(kMovingTargetHeight);
    world.StoreCandidate(handle, candidate.value());
  }
  world.IssueCommand(kAttackCommand);
  if (world.RequiresCooldown(handle)) {
    const auto delay = world.UsesRoomDelay()
                           ? world.RoomDelay()
                           : std::optional<std::uint8_t>{kFixedDelay};
    if (!delay) return Refuse("AI action cooldown requires a room");
    world.SetDelay(delay.value());
  }
  return 1;
}

// decomp: func_800DAD9C (src/BATTLE/BATTLE.PRG/71618.c:1093).
std::optional<int> TryExecutePlannedActions(ActionExecutionWorld& world) {
  for (int rank = 0;; ++rank) {
    const int count = world.CandidateCount();
    if (count < 0 || count > static_cast<int>(kCandidateCapacity)) {
      return Refuse("AI execution candidate count is outside its ranking");
    }
    if (rank >= count) break;
    const auto handle = world.RankedCandidate(rank);
    if (!handle) return Refuse("AI execution ranked candidate is missing");
    const auto result = TryExecuteActionCandidate(world, handle.value());
    if (!result || result.value() != 0) return result;
  }
  return ExecuteFallback(world);
}

// decomp: func_800DAED0 (src/BATTLE/BATTLE.PRG/71618.c:1128).
bool UpdateActorDecisions(DecisionUpdateWorld& world) {
  const auto head = world.ActorHead();
  if (head == 0 || !world.ActorId(head)) {
    return RefuseUpdate("AI decisions require the battle list head");
  }
  std::array<std::size_t, kCandidateTargetSlots> visited{};
  std::size_t count = 0;
  for (auto node = world.ActorNext(head); node != 0;
       node = world.ActorNext(node)) {
    if (node == head) {
      return RefuseUpdate("AI decision list returns to its head");
    }
    if (count == visited.size()) {
      return RefuseUpdate("AI decision list is unbounded");
    }
    for (std::size_t index = 0; index < count; ++index) {
      if (visited.at(index) == node) {
        return RefuseUpdate("AI decision list is cyclic");
      }
    }
    visited.at(count++) = node;
    const auto id = world.ActorId(node);
    if (!id || id.value() < 0 || id.value() >= kCandidateTargetSlots ||
        !world.SelectActor(id.value())) {
      return RefuseUpdate(
          "AI decision actor has an invalid ID or missing state");
    }
    if (!UpdateSelected(world)) return false;
  }
  return true;
}
}  // namespace VS::Battle::Ai
