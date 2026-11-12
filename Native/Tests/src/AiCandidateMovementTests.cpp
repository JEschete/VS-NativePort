#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/CandidateMovement.hpp"
#include "Battle/Ai/CandidateMovementLive.hpp"
#include "Content/ModuleData.h"

extern "C" {
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
}

namespace {
namespace ai = VS::Battle::Ai;
enum class CallKind {
  kCanApproach,
  kCooldown,
  kEngage,
  kDisengage,
  kApproach,
  kTrack,
  kFlee,
  kWander,
  kMove,
  kFind,
  kRelease,
  kRecover,
  kRadial,
  kDispatch,
  kRandom,
  kAwareness,
  kGround
};

struct Call {
  CallKind kind;
  std::int64_t first = 0;
  std::int64_t second = 0;
  std::int64_t third = 0;
  std::uint8_t target = 0;
  std::uint8_t gait = 0;
  std::uint8_t delay = 0;
};

ai::ActionCandidate Candidate(unsigned target, int distance = 10000,
                              unsigned threshold = 0) {
  ai::ActionCandidate candidate;
  candidate.SetTargetId(target);
  candidate.SetDistanceSquared(distance);
  candidate.SetThreshold(threshold);
  return candidate;
}

template <typename T>
T Pop(std::deque<T>& queue, T empty) {
  if (queue.empty()) return empty;
  T value = queue.front();
  queue.pop_front();
  return value;
}

template <typename T>
T& FixtureValue(std::optional<T>& value) {
  if (!value) {
    throw std::logic_error("Candidate movement fixture value is absent");
  }
  return value.value();
}

class World final : public ai::CandidateMovementWorld {
 public:
  std::array<std::optional<ai::ActionCandidate>, ai::kCandidateCapacity>
      entries{};
  std::array<std::optional<std::size_t>, ai::kCandidateCapacity> ranks{};
  std::array<bool, ai::kCandidateTargetSlots> frame{};
  std::array<std::optional<ai::PursuitTarget>, ai::kCandidateTargetSlots>
      models{};
  std::array<std::optional<std::int16_t>, ai::kCandidateTargetSlots> heights{};
  std::optional<std::uint32_t> frame_distance = 0x24000;
  std::optional<ai::ActorRoster> roster = ai::ActorRoster{};
  std::uint16_t readiness = 100;
  std::uint32_t approach_moves = 0;
  bool can_approach = true;
  bool cooldown = true;
  int ground = 0;
  int random = 0;
  std::deque<int> approaches, tracks, flees;
  std::deque<ai::RelatedActor> related;
  std::function<void(ai::CandidateMover&)> after_approach;
  std::function<void(ai::CandidateMover&)> after_dispatch;
  std::function<void()> after_can_approach;
  std::function<void()> after_cooldown;
  std::function<void(ai::CandidateMover&)> after_flee;
  mutable std::vector<Call> calls;

  World() {
    entries.at(0) = ::Candidate(2);
    entries.at(1) = ::Candidate(3, 40000);
    ranks.at(0) = 0;
    ranks.at(1) = 1;
    frame.fill(true);
    models.fill(ai::PursuitTarget{});
    heights.fill(0);
  }
  std::size_t ActorHead() const override {
    return roster && roster.value().count != 0 ? 1 : 0;
  }
  std::optional<int> ActorId(std::size_t node) const override {
    if (!roster || node == 0 || node > roster.value().count) {
      return std::nullopt;
    }
    return roster.value().ids.at(node - 1);
  }
  std::size_t ActorNext(std::size_t node) const override {
    return roster && node < roster.value().count ? node + 1 : 0;
  }
  std::optional<std::size_t> RankedHandle(std::size_t rank) const override {
    return rank < ranks.size() ? ranks.at(rank) : std::nullopt;
  }
  std::optional<ai::ActionCandidate> Candidate(
      std::size_t handle) const override {
    return handle < entries.size() ? entries.at(handle) : std::nullopt;
  }
  bool HasFrameActor(int target) const override { return frame.at(target); }
  std::optional<ai::PursuitTarget> TargetModel(int target) const override {
    return models.at(target);
  }
  std::optional<std::int16_t> TargetHeight(int target) const override {
    return heights.at(target);
  }
  std::optional<std::uint32_t> FrameDistance(int, int) const override {
    return frame_distance;
  }
  std::optional<ai::ActorRoster> BattleActors() const override {
    return roster;
  }
  int GroundHeight(int x, int z) const override {
    calls.push_back({CallKind::kGround, x, z});
    return ground;
  }
  std::uint16_t ReadinessLevel() const override { return readiness; }
  void CountApproachMove() override { ++approach_moves; }
  bool CanApproach(ai::CandidateMover& mover, std::size_t handle) override {
    Note(CallKind::kCanApproach, mover, static_cast<std::int64_t>(handle));
    if (after_can_approach) after_can_approach();
    return can_approach;
  }
  bool CooldownReady(ai::CandidateMover& mover, std::size_t handle) override {
    Note(CallKind::kCooldown, mover, static_cast<std::int64_t>(handle));
    if (after_cooldown) after_cooldown();
    return cooldown;
  }
  void TryEngage(ai::CandidateMover& mover) override {
    Note(CallKind::kEngage, mover);
  }
  void Disengage(ai::CandidateMover& mover) override {
    Note(CallKind::kDisengage, mover);
  }
  int Approach(ai::CandidateMover& mover,
               const ai::ApproachRequest& request) override {
    Note(CallKind::kApproach, mover, request.target, request.distance,
         request.option);
    if (after_approach) after_approach(mover);
    return Pop(approaches, 0);
  }
  int Track(ai::CandidateMover& mover, int target) override {
    Note(CallKind::kTrack, mover, target);
    return Pop(tracks, 0);
  }
  int Flee(ai::CandidateMover& mover, int kind) override {
    Note(CallKind::kFlee, mover, kind);
    if (after_flee) after_flee(mover);
    return Pop(flees, 0);
  }
  void Wander(ai::CandidateMover& mover, std::uint32_t tile,
              int radius) override {
    Note(CallKind::kWander, mover, tile, radius);
  }
  void MoveToTile(ai::CandidateMover& mover, std::uint32_t tile,
                  int radius) override {
    Note(CallKind::kMove, mover, tile, radius);
  }
  ai::RelatedActor FindRelated(ai::CandidateMover& mover, int relation,
                               bool want_tile) override {
    Note(CallKind::kFind, mover, relation, want_tile ? 1 : 0);
    return Pop(related, ai::RelatedActor{});
  }
  void ReleaseReservations(ai::CandidateMover& mover) override {
    Note(CallKind::kRelease, mover);
  }
  void Recover(ai::CandidateMover& mover) override {
    Note(CallKind::kRecover, mover);
  }
  void ProjectRadial(ai::CandidateMover& mover, std::uint32_t tile, int angle,
                     std::uint32_t distance) override {
    Note(CallKind::kRadial, mover, tile, angle, distance);
  }
  void DispatchLocomotion(ai::CandidateMover& mover, int kind,
                          int angle) override {
    Note(CallKind::kDispatch, mover, kind, angle);
    if (after_dispatch) after_dispatch(mover);
  }
  int RandomMasked(int mask) override {
    calls.push_back({CallKind::kRandom, mask});
    return random;
  }
  void SetAwareness(ai::CandidateMover& mover, int marker,
                    int frames) override {
    Note(CallKind::kAwareness, mover, marker, frames);
  }
  std::size_t Count(CallKind kind) const {
    return static_cast<std::size_t>(
        std::ranges::count(calls, kind, &Call::kind));
  }
  const Call& Last(CallKind kind) const {
    return *std::find_if(
        calls.rbegin(), calls.rend(),
        [kind](const Call& call) { return call.kind == kind; });
  }

 private:
  void Note(CallKind kind, const ai::CandidateMover& mover,
            std::int64_t first = 0, std::int64_t second = 0,
            std::int64_t third = 0) {
    calls.push_back({kind, first, second, third, mover.movement_target,
                     mover.gait, mover.guard_delay});
  }
};

ai::CandidateMover Mover() {
  ai::CandidateMover mover;
  mover.actor_id = 1;
  mover.candidate_count = 1;
  mover.plan_mode = 1;
  mover.linked_actor = 2;
  return mover;
}
}  // namespace

TEST_CASE("Escort follow uses its distance and recovery prefix",
          "[candidate-movement][F01656]") {
  World world;
  auto mover = Mover();
  world.can_approach = false;
  world.approaches = {1};
  mover.range_mode = 1;
  CHECK(ai::UpdateEscortCombatBehavior(world, mover) == 1);
  CHECK(world.Last(CallKind::kApproach).second == 384);
  CHECK(world.Last(CallKind::kApproach).third == 0);
  CHECK(mover.movement_target == 2);
  CHECK(world.Count(CallKind::kRelease) == 1);
  CHECK(world.Count(CallKind::kRecover) == 1);
}

TEST_CASE("Escort restores current old-class bits after flee mutations",
          "[candidate-movement][F01656]") {
  World world;
  auto mover = Mover();
  mover.action_status = 1;
  mover.linked_actor = 16;
  FixtureValue(world.roster).count = 3;
  FixtureValue(world.roster).ids = {2, 3, 4};
  mover.relations.at(2) = 0x001000C0;
  mover.relations.at(3) = 0x20000080;
  mover.relations.at(4) = 0x10100040;
  world.after_flee = [](auto& state) { state.relations.at(4) = 0x2FE123FF; };
  world.flees = {-7};
  CHECK(ai::UpdateEscortCombatBehavior(world, mover) == 1);
  CHECK(world.Count(CallKind::kFlee) == 1);
  CHECK(world.Last(CallKind::kFlee).first == 0);
  CHECK(mover.relations.at(4) == 0x2FE123BF);
  CHECK(mover.relations.at(2) == 0x001000C0);
  CHECK(mover.relations.at(3) == 0x20000080);
}

TEST_CASE("Escort exhaustion restores relations and refuses invalid IDs",
          "[candidate-movement][F01656]") {
  World world;
  auto mover = Mover();
  mover.action_status = 1;
  mover.linked_actor = 16;
  FixtureValue(world.roster).count = 1;
  FixtureValue(world.roster).ids.at(0) = 3;
  mover.relations.at(3) = 0x20100000;
  CHECK(ai::UpdateEscortCombatBehavior(world, mover) == 0);
  CHECK(mover.relations.at(3) == 0x20100080);
  mover.linked_actor = -1;
  CHECK_FALSE(ai::UpdateEscortCombatBehavior(world, mover));
  mover.linked_actor = 16;
  FixtureValue(world.roster).ids.at(0) = 16;
  CHECK_FALSE(ai::UpdateEscortCombatBehavior(world, mover));
}

TEST_CASE("Candidate movement does not read an unused candidate",
          "[candidate-movement][F01649]") {
  World world;
  auto mover = Mover();
  mover.candidate_count = 0;
  CHECK(ai::MoveForActionCandidate(world, mover, 99) == 0);
  mover.candidate_count = 1;
  CHECK_FALSE(ai::MoveForActionCandidate(world, mover, 99));
  world.frame.at(2) = false;
  CHECK(ai::MoveForActionCandidate(world, mover, 0) == 0);
  CHECK(world.calls.empty());
}

TEST_CASE(
    "Candidate movement treats allied targets as immediately approachable",
    "[candidate-movement][F01649]") {
  World world;
  auto mover = Mover();
  mover.relations.at(2) = 0x81;
  mover.distances_squared.at(2) = 9800;
  world.approaches = {1};
  CHECK(ai::MoveForActionCandidate(world, mover, 0) == 1);
  CHECK(world.Count(CallKind::kCanApproach) == 0);
  CHECK(world.approach_moves == 1);
  CHECK(world.Count(CallKind::kEngage) == 1);
  CHECK(world.Last(CallKind::kApproach).second == 99);
  CHECK(world.Last(CallKind::kApproach).third == 0);
  CHECK(world.Count(CallKind::kTrack) == 0);
}

TEST_CASE(
    "Candidate movement engagement range is strict and honors the range flag",
    "[candidate-movement][F01649]") {
  for (const auto distance : {9800U, 9801U, 65028095U, 65028096U}) {
    World world;
    auto mover = Mover();
    mover.relations.at(2) = 1;
    mover.distances_squared.at(2) = distance;
    mover.movement_flags = distance > 10000 ? 8 : 0;
    CHECK(ai::MoveForActionCandidate(world, mover, 0) == 0);
    CHECK(world.Count(CallKind::kEngage) ==
          (distance == 9800 || distance == 65028095 ? 1 : 0));
    CHECK(world.Last(CallKind::kApproach).third == 1);
  }
}

TEST_CASE(
    "Candidate movement uses the minimum distance when engagement is refused",
    "[candidate-movement][F01649]") {
  World world;
  auto mover = Mover();
  world.can_approach = false;
  world.tracks = {-4};
  CHECK(ai::MoveForActionCandidate(world, mover, 0) == 1);
  CHECK(world.approach_moves == 0);
  CHECK(world.Last(CallKind::kApproach).second == 640);
  CHECK(world.Last(CallKind::kTrack).first == 2);
}

TEST_CASE("Candidate destination switches from near wandering to direct travel",
          "[candidate-movement][F01649]") {
  for (const int x : {319, 320}) {
    World world;
    auto mover = Mover();
    mover.plan_mode = 2;
    mover.position = {static_cast<std::int16_t>(x), 0, 64};
    mover.planned_tile = 0x55030402;
    CHECK(ai::MoveForActionCandidate(world, mover, 0) == 1);
    const auto kind = x == 319 ? CallKind::kWander : CallKind::kMove;
    CHECK(world.Last(kind).first == mover.planned_tile);
    CHECK(world.Last(kind).second == (x == 319 ? 2 : 256));
    CHECK(world.Last(kind).gait == (x == 319 ? 0 : 1));
    CHECK(world.Last(kind).target == (x == 319 ? 2 : 0));
  }
}

TEST_CASE("Candidate destination honors minimum range equality and cooldown",
          "[candidate-movement][F01649]") {
  for (const bool ready : {false, true}) {
    World world;
    auto mover = Mover();
    mover.plan_mode = 2;
    mover.position = {64, 0, 64};
    world.cooldown = ready;
    FixtureValue(world.entries.at(0))
        .SetOption(ai::CandidateOption::kMinimumRange, true);
    FixtureValue(world.entries.at(0)).SetRange(35);
    FixtureValue(world.models.at(2)).position = {64, 0, 0};
    CHECK(ai::MoveForActionCandidate(world, mover, 0) == (ready ? 1 : 0));
    CHECK(world.Count(CallKind::kWander) == (ready ? 1 : 0));
    CHECK(world.Count(CallKind::kApproach) == (ready ? 0 : 1));
  }
}

TEST_CASE("Candidate destination refuses a missing target height or model",
          "[candidate-movement][F01649]") {
  World world;
  auto mover = Mover();
  mover.plan_mode = 2;
  FixtureValue(world.entries.at(0))
      .SetOption(ai::CandidateOption::kMinimumRange, true);
  world.heights.at(2).reset();
  CHECK_FALSE(ai::MoveForActionCandidate(world, mover, 0));
  world.heights.at(2) = 0;
  world.models.at(2).reset();
  CHECK_FALSE(ai::MoveForActionCandidate(world, mover, 0));
  CHECK(world.Count(CallKind::kMove) == 0);
  CHECK(world.Count(CallKind::kWander) == 0);
}

TEST_CASE(
    "Candidate movement rereads gate writes but retains its original target "
    "and limit",
    "[candidate-movement][F01649]") {
  World world;
  auto mover = Mover();
  mover.distances_squared.at(2) = 9800;
  mover.relations.at(3) = 1;
  world.after_can_approach = [&] {
    FixtureValue(world.entries.at(0)).SetDistanceSquared(16384);
    FixtureValue(world.entries.at(0)).SetTargetId(3);
  };
  CHECK(ai::MoveForActionCandidate(world, mover, 0) == 0);
  CHECK(world.Last(CallKind::kApproach).first == 2);
  CHECK(world.Last(CallKind::kApproach).second == 128);
  CHECK(world.Count(CallKind::kEngage) == 1);
}

TEST_CASE(
    "Candidate cooldown refreshes options while destination reads the original "
    "target",
    "[candidate-movement][F01649]") {
  World world;
  auto mover = Mover();
  mover.plan_mode = 2;
  mover.relations.at(2) = 0x80;
  FixtureValue(world.models.at(2)).position = {64, 0, 0};
  world.models.at(3).reset();
  world.after_cooldown = [&] {
    auto& candidate = FixtureValue(world.entries.at(0));
    candidate.SetOption(ai::CandidateOption::kMinimumRange, true);
    candidate.SetDistanceSquared(16384);
    candidate.SetRange(1);
    candidate.SetTargetId(3);
  };
  world.approaches = {1};
  CHECK(ai::MoveForActionCandidate(world, mover, 0) == 1);
  CHECK(world.Last(CallKind::kApproach).first == 2);
  CHECK(world.Last(CallKind::kApproach).second == 128);
  CHECK(world.Count(CallKind::kWander) == 0);
}

TEST_CASE(
    "Candidate flee preserves the callee result and player mode disengages",
    "[candidate-movement][F01649]") {
  World world;
  auto mover = Mover();
  mover.plan_mode = 4;
  world.flees = {-3};
  CHECK(ai::MoveForActionCandidate(world, mover, 0) == -3);
  CHECK(world.Last(CallKind::kFlee).first == 0);
  mover.plan_mode = 7;
  mover.relations.at(0) = 0xC0;
  CHECK(ai::MoveForActionCandidate(world, mover, 0) == 0);
  CHECK(world.Count(CallKind::kDisengage) == 1);
  CHECK(world.Last(CallKind::kApproach).first == 0);
  CHECK(world.Last(CallKind::kApproach).second == 640);
}

TEST_CASE("Combat pursuit selects only the first eligible different target",
          "[candidate-movement][F01650]") {
  World world;
  auto mover = Mover();
  mover.candidate_count = 4;
  world.entries.at(1) = Candidate(2);
  world.entries.at(2) = Candidate(3, 40000, 100);
  world.entries.at(3) = Candidate(4, 90000, 99);
  world.ranks.at(2) = 2;
  world.ranks.at(3) = 3;
  CHECK(ai::UpdateCombatPursuit(world, mover) == 0);
  CHECK(world.Count(CallKind::kApproach) == 2);
  CHECK(world.Last(CallKind::kApproach).first == 4);
  CHECK(world.Last(CallKind::kApproach).second == 298);
}

TEST_CASE("Combat pursuit refuses malformed candidate ranking",
          "[candidate-movement][F01650]") {
  for (const int count : {-1, 2, 17}) {
    World world;
    auto mover = Mover();
    mover.candidate_count = count;
    world.ranks.at(1).reset();
    CHECK_FALSE(ai::UpdateCombatPursuit(world, mover));
  }
}

TEST_CASE(
    "Pending pursuit uses second distance but retains first target on "
    "fallthrough",
    "[candidate-movement][F01650]") {
  World world;
  auto mover = Mover();
  mover.action_status = 1;
  mover.candidate_count = 2;
  CHECK(ai::UpdateCombatPursuit(world, mover) == 0);
  REQUIRE(world.calls.size() == 6);
  CHECK(world.calls.at(1).first == 2);
  CHECK(world.calls.at(1).second == 199);
  CHECK(world.calls.at(1).third == 0);
  CHECK(world.Last(CallKind::kApproach).first == 2);
  CHECK(world.Last(CallKind::kApproach).second == 640);
  CHECK(world.Count(CallKind::kDisengage) == 2);
}

TEST_CASE(
    "Combat pursuit nearest selection is unsigned and keeps first equal "
    "distance",
    "[candidate-movement][F01650]") {
  World world;
  auto mover = Mover();
  mover.action_status = 2;
  FixtureValue(world.roster).ids = {3, 2, 4};
  FixtureValue(world.roster).count = 3;
  mover.relations.at(2) = mover.relations.at(3) = mover.relations.at(4) = 0xC0;
  mover.distances_squared.at(2) = mover.distances_squared.at(3) = 50;
  mover.distances_squared.at(4) = 0xFFFFFFFF;
  CHECK(ai::UpdateCombatPursuit(world, mover) == 0);
  CHECK(world.Last(CallKind::kApproach).first == 3);
  FixtureValue(world.roster).ids.at(0) = 16;
  CHECK_FALSE(ai::UpdateCombatPursuit(world, mover));
}

TEST_CASE("Combat pursuit tries Ashley before candidates or the roster",
          "[candidate-movement][F01650]") {
  for (const auto status : {1, 2}) {
    World world;
    auto mover = Mover();
    mover.action_status = static_cast<std::uint8_t>(status);
    mover.relations.at(0) = 0xC0;
    world.approaches = {5};
    world.ranks.at(0).reset();
    world.roster.reset();
    CHECK(ai::UpdateCombatPursuit(world, mover) == 1);
    CHECK(world.Last(CallKind::kApproach).first == 0);
    CHECK(world.Count(CallKind::kTrack) == 0);
  }
}

TEST_CASE(
    "Planned target approach counts engagement only after the gate passes",
    "[candidate-movement][F01651]") {
  for (const bool allowed : {false, true}) {
    World world;
    auto mover = Mover();
    mover.max_range = 1;
    mover.relations.at(2) = 1;
    mover.distances_squared.at(2) = 17160;
    world.can_approach = allowed;
    world.approach_moves = std::numeric_limits<std::uint32_t>::max();
    world.tracks = {7};
    CHECK(ai::TryApproachPlannedTarget(world, mover, 0) == (allowed ? 1 : 0));
    CHECK(world.Count(CallKind::kEngage) == (allowed ? 1 : 0));
    CHECK(world.approach_moves ==
          (allowed ? 0 : std::numeric_limits<std::uint32_t>::max()));
  }
}

TEST_CASE("Planned target approach excludes the exact enlarged range boundary",
          "[candidate-movement][F01651]") {
  World world;
  auto mover = Mover();
  mover.max_range = 1;
  mover.relations.at(2) = 1;
  mover.distances_squared.at(2) = 17161;
  world.approaches = {1};
  CHECK(ai::TryApproachPlannedTarget(world, mover, 0) == 1);
  CHECK(world.Count(CallKind::kEngage) == 0);
  CHECK(world.Last(CallKind::kApproach).second == 99);
  world.entries.at(0).reset();
  CHECK_FALSE(ai::TryApproachPlannedTarget(world, mover, 0));
}

TEST_CASE(
    "Close follow first tries the planned action and preserves its candidate "
    "handle",
    "[candidate-movement][F01652]") {
  World world;
  auto mover = Mover();
  world.approaches = {1};
  CHECK(ai::UpdateCloseFollowBehavior(world, mover) == 1);
  CHECK(world.Count(CallKind::kApproach) == 1);
  CHECK(world.Last(CallKind::kApproach).second == 99);
  CHECK(world.Count(CallKind::kRelease) == 0);
}

TEST_CASE(
    "Close follow uses post-approach state and publishes target before "
    "recovery",
    "[candidate-movement][F01652]") {
  World world;
  auto mover = Mover();
  mover.action_status = 1;
  mover.linked_actor = 4;
  world.approaches = {1};
  world.after_approach = [](ai::CandidateMover& state) {
    state.range_mode = 1;
  };
  CHECK(ai::UpdateCloseFollowBehavior(world, mover) == 1);
  CHECK(world.Last(CallKind::kApproach).first == 4);
  CHECK(world.Last(CallKind::kApproach).second == 288);
  CHECK(world.Last(CallKind::kRelease).target == 2);
  CHECK(world.Last(CallKind::kRecover).target == 2);
  CHECK(world.calls.at(1).kind == CallKind::kRelease);
  CHECK(world.calls.at(2).kind == CallKind::kRecover);
}

TEST_CASE("Close follow handles no link, negative link and tracking fallback",
          "[candidate-movement][F01652]") {
  World world;
  auto mover = Mover();
  mover.action_status = 1;
  mover.linked_actor = 16;
  CHECK(ai::UpdateCloseFollowBehavior(world, mover) == 0);
  mover.linked_actor = -1;
  CHECK_FALSE(ai::UpdateCloseFollowBehavior(world, mover));
  mover.linked_actor = 2;
  world.tracks = {-2};
  CHECK(ai::UpdateCloseFollowBehavior(world, mover) == 1);
  CHECK(world.Count(CallKind::kRelease) == 0);
}

TEST_CASE("Enemy pursuit flees directly or requires both relationship searches",
          "[candidate-movement][F01653]") {
  World world;
  auto mover = Mover();
  mover.fleeing = 1;
  world.flees = {-4};
  CHECK(ai::UpdateEnemyPursuitBehavior(world, mover) == -4);
  CHECK(world.Count(CallKind::kFind) == 0);
  mover.fleeing = 0;
  world.related = {{3, 0}, {2, 0}};
  world.approaches = {1};
  CHECK(ai::UpdateEnemyPursuitBehavior(world, mover) == 1);
  CHECK(world.Last(CallKind::kApproach).first == 3);
  CHECK(world.Last(CallKind::kApproach).second == 1024);
  CHECK(world.Last(CallKind::kFind).first == 2);
}

TEST_CASE("Close follow only requires the candidate when it is read",
          "[candidate-movement][F01652]") {
  World world;
  auto mover = Mover();
  world.ranks.at(0).reset();
  mover.action_status = 1;
  mover.linked_actor = 16;
  CHECK(ai::UpdateCloseFollowBehavior(world, mover) == 0);
  mover.linked_actor = 2;
  world.approaches = {1};
  CHECK(ai::UpdateCloseFollowBehavior(world, mover) == 1);
  world.approaches = {1};
  mover.range_mode = 1;
  CHECK_FALSE(ai::UpdateCloseFollowBehavior(world, mover));
  CHECK(world.Count(CallKind::kRelease) == 0);
}

TEST_CASE("Enemy pursuit falls back when no enemy or ally is available",
          "[candidate-movement][F01653]") {
  for (const int enemy : {-1, 3}) {
    World world;
    auto mover = Mover();
    mover.action_status = 3;
    world.related = {{enemy, 0}, {-1, 0}};
    CHECK(ai::UpdateEnemyPursuitBehavior(world, mover) == 0);
    CHECK(world.Count(CallKind::kFind) == (enemy < 0 ? 1 : 2));
    CHECK(world.Count(CallKind::kApproach) == 0);
  }
}

TEST_CASE("Guard movement projects onto the protected actor's threat side",
          "[candidate-movement][F01654]") {
  World world;
  auto mover = Mover();
  mover.relations.at(2) = 0xA0;
  mover.position = {64, 0, 256};
  world.models.at(2) = ai::PursuitTarget{{64, 0, 64}, 0};
  world.related = {{3, 0x00020000}};
  CHECK(ai::UpdateGuardMovementBehavior(world, mover) == 1);
  CHECK(mover.guarding == 1);
  CHECK(mover.gait == 1);
  CHECK(mover.guard_delay == 10);
  CHECK(world.Last(CallKind::kRadial).third == 320);
  CHECK(world.Last(CallKind::kDispatch).gait == 1);
  CHECK(world.Count(CallKind::kApproach) == 0);
}

TEST_CASE("Guard delay permits opposed movement until its last tick",
          "[candidate-movement][F01654]") {
  for (const int delay : {1, 2}) {
    World world;
    auto mover = Mover();
    mover.relations.at(2) = 0xA0;
    mover.guard_delay = static_cast<std::uint8_t>(delay);
    mover.position = {64, 0, -256};
    world.models.at(2) = ai::PursuitTarget{{64, 0, 64}, 0};
    world.related = {{3, 0x00020000}};
    CHECK(ai::UpdateGuardMovementBehavior(world, mover) ==
          (delay == 2 ? 1 : 0));
    CHECK(world.Count(CallKind::kRadial) == (delay == 2 ? 1 : 0));
    CHECK(mover.guard_delay == delay - 1);
  }
}

TEST_CASE("Guard movement retains a delay written by locomotion",
          "[candidate-movement][F01654]") {
  World world;
  auto mover = Mover();
  mover.relations.at(2) = 0xA0;
  mover.position = {64, 0, 256};
  world.models.at(2) = ai::PursuitTarget{{64, 0, 64}, 0};
  world.related = {{3, 0x00020000}};
  world.after_dispatch = [](ai::CandidateMover& state) {
    state.guard_delay = 7;
  };
  CHECK(ai::UpdateGuardMovementBehavior(world, mover) == 1);
  CHECK(mover.guard_delay == 7);
}

TEST_CASE(
    "Guard movement recovers at the inclusive protected-distance boundary",
    "[candidate-movement][F01654]") {
  for (const auto distance : {0x23FFFU, 0x24000U}) {
    World world;
    auto mover = Mover();
    mover.relations.at(2) = 0x80;
    world.related = {{3, 0}};
    world.frame_distance = distance;
    CHECK(ai::UpdateGuardMovementBehavior(world, mover) ==
          (distance == 0x23FFF ? 1 : 0));
    CHECK(world.Count(CallKind::kRecover) == (distance == 0x23FFF ? 1 : 0));
    CHECK(mover.movement_target == (distance == 0x23FFF ? 2 : 0));
  }
}

TEST_CASE("Guard fallback clears guard state and preserves both flee calls",
          "[candidate-movement][F01654]") {
  World world;
  auto mover = Mover();
  mover.guarding = 2;
  world.related = {{3, 0}};
  world.random = 127;
  world.flees = {0, -7};
  CHECK(ai::UpdateGuardMovementBehavior(world, mover) == -7);
  CHECK(mover.guarding == 0);
  CHECK(mover.alerted == 1);
  CHECK(world.Last(CallKind::kRandom).first == 128);
  CHECK(world.Last(CallKind::kAwareness).first == 6);
  CHECK(world.Last(CallKind::kAwareness).second == 0);
  CHECK(world.Count(CallKind::kFlee) == 2);
}

TEST_CASE("Guard fallback random comparison is unsigned and excludes 128",
          "[candidate-movement][F01654]") {
  for (const int random : {-1, 128}) {
    World world;
    auto mover = Mover();
    mover.guarding = 1;
    world.related = {{3, 0}};
    world.random = random;
    world.flees = {2};
    CHECK(ai::UpdateGuardMovementBehavior(world, mover) == 1);
    CHECK(mover.alerted == 0);
    CHECK(world.Count(CallKind::kFlee) == 1);
  }
}

TEST_CASE("Guard movement refuses invalid targets and required records",
          "[candidate-movement][F01654]") {
  World world;
  auto mover = Mover();
  mover.linked_actor = -1;
  CHECK_FALSE(ai::UpdateGuardMovementBehavior(world, mover));
  mover.linked_actor = 2;
  mover.relations.at(2) = 0xA0;
  world.related = {{3, 0}};
  world.models.at(2).reset();
  CHECK_FALSE(ai::UpdateGuardMovementBehavior(world, mover));
  mover.relations.at(2) = 0x80;
  world.related = {{3, 0}};
  world.frame_distance.reset();
  CHECK_FALSE(ai::UpdateGuardMovementBehavior(world, mover));
}

TEST_CASE(
    "Candidate movement live empty path does not require a candidate or alter "
    "state",
    "[candidate-movement][live]") {
  Battle_Ai_ActorAiState state{};
  state.opaque1 = 8;
  state.opaqueF = 9;
  state.opaque44 = 10;
  state.movementTarget = 7;
  CHECK(ai::MoveLiveForActionCandidate(
            reinterpret_cast<Battle_Ai_CandidateMovementView*>(&state),
            nullptr) == 0);
  CHECK(state.opaque1 == 8);
  CHECK(state.opaqueF == 9);
  CHECK(state.opaque44 == 10);
  CHECK(state.movementTarget == 7);
}

TEST_CASE(
    "Candidate movement module entries bind to the six checked live functions",
    "[candidate-movement][live]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [&](std::uint32_t address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  CHECK(resolve(0x800D9E18) ==
        reinterpret_cast<void*>(&ai::MoveLiveForActionCandidate));
  CHECK(resolve(0x800DA1D4) ==
        reinterpret_cast<void*>(&ai::UpdateLiveCombatPursuit));
  CHECK(resolve(0x800DA4BC) ==
        reinterpret_cast<void*>(&ai::TryApproachLivePlannedTarget));
  CHECK(resolve(0x800DA5CC) ==
        reinterpret_cast<void*>(&ai::UpdateLiveCloseFollowBehavior));
  CHECK(resolve(0x800DA6A4) ==
        reinterpret_cast<void*>(&ai::UpdateLiveEnemyPursuitBehavior));
  CHECK(resolve(0x800DA738) ==
        reinterpret_cast<void*>(&ai::UpdateLiveGuardMovementBehavior));
  CHECK(resolve(0x800DAAD8) ==
        reinterpret_cast<void*>(&ai::UpdateLiveEscortCombatBehavior));
}
