#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/DecisionPrimitives.hpp"
#include "Battle/Ai/TargetApproach.hpp"
#include "Battle/Ai/TargetApproachLive.hpp"
#include "Content/ModuleData.h"
#include "Core/Math/ScalarMath.hpp"

namespace {
namespace ai = VS::Battle::Ai;
using ai::CommandIntent;

constexpr std::uint8_t kActor = 3;
constexpr std::uint8_t kTarget = 5;
constexpr std::uint32_t kTag = 0x5A000700;
constexpr std::uint32_t kActive = ai::kApproachRelationActive;
constexpr std::uint32_t kWasActive = ai::kApproachRelationWasActive;
constexpr std::uint32_t kReachable = ai::kApproachRelationReachable;
constexpr std::uint32_t kVisible = ai::kApproachRelationVisible;
constexpr std::int32_t kRange = 300;
// Squared distances inside and outside the range's bands for kRange: far
// until (300 - 96)^2, close within (300 - 128)^2, back to in range beyond
// (300 - 32)^2 and out of range beyond 300^2.
constexpr std::uint32_t kFarBand = 204 * 204;
constexpr std::uint32_t kCloseBand = 172 * 172;
constexpr std::uint32_t kLeaveCloseBand = 268 * 268;
constexpr std::uint32_t kRangeBand = 300 * 300;
constexpr std::uint16_t kHoldDistance = 8;
constexpr std::uint8_t kFar = 0;
constexpr std::uint8_t kInRange = 1;
constexpr std::uint8_t kClose = 2;

constexpr std::uint32_t Tile(int x, int z, std::uint32_t other = 0) {
  return other | (static_cast<std::uint32_t>(z) << 16) |
         static_cast<std::uint32_t>(x);
}

constexpr std::int16_t Centre(int tile) {
  return static_cast<std::int16_t>((tile * 128) + 64);
}

enum class Kind {
  kCommand,
  kProject,
  kDispatch,
  kRecover,
  kAwareness,
  kHesitate,
  kClearWalk,
  kRefresh
};

// One world call and its arguments.
struct Call {
  Kind kind;
  std::int64_t a = 0;
  std::int64_t b = 0;
  std::int64_t c = 0;

  friend bool operator==(const Call&, const Call&) = default;
};
using Calls = std::vector<Call>;

Call Command(CommandIntent intent) {
  return {Kind::kCommand, static_cast<int>(intent)};
}
Call Project(std::uint32_t source, int angle, std::uint32_t distance) {
  return {Kind::kProject, source, angle, distance};
}
Call Dispatch(int kind, int angle) { return {Kind::kDispatch, kind, angle}; }
// Recovery records the movement target it was handed.
Call Recover(int movement_target) { return {Kind::kRecover, movement_target}; }
Call Awareness(int marker, int frames) {
  return {Kind::kAwareness, marker, frames};
}
Call Hesitate(int chance, int kind, int slot) {
  return {Kind::kHesitate, chance, kind, slot};
}
Call ClearWalk() { return {Kind::kClearWalk}; }
Call Refresh() { return {Kind::kRefresh}; }

struct FakeApproachWorld final : ai::TargetApproachWorld {
  std::array<std::optional<ai::ApproachTargetModel>, 17> models{};
  std::optional<std::uint8_t> opcode = 0;
  // Queued hesitation answers; `otherwise` once they run out.
  std::vector<int> hesitations;
  std::size_t next_hesitation = 0;
  int otherwise = 0;
  // The destination a projection sets.
  std::uint32_t projected = Tile(11, 10);
  Calls calls;

  [[nodiscard]] std::optional<ai::ApproachTargetModel> TargetModel(
      int actor_id) const override {
    if (actor_id < 0 || static_cast<std::size_t>(actor_id) >= models.size()) {
      return std::nullopt;
    }
    return models.at(static_cast<std::size_t>(actor_id));
  }
  [[nodiscard]] std::optional<std::uint8_t> CommandOpcode() const override {
    return opcode;
  }
  void IssueCommand(ai::TargetApproacher& /*approacher*/,
                    CommandIntent intent) override {
    calls.push_back(Command(intent));
  }
  void ProjectClearRadialDestination(ai::TargetApproacher& approacher,
                                     std::uint32_t source, int angle,
                                     std::uint32_t distance) override {
    calls.push_back(Project(source, angle, distance));
    approacher.destination = projected;
  }
  void DispatchLocomotion(ai::TargetApproacher& /*approacher*/, int kind,
                          int angle) override {
    calls.push_back(Dispatch(kind, angle));
  }
  void RecoverMovement(ai::TargetApproacher& approacher) override {
    calls.push_back(Recover(approacher.movement_target));
  }
  void SetTemporaryAwareness(ai::TargetApproacher& /*approacher*/, int marker,
                             int frames) override {
    calls.push_back(Awareness(marker, frames));
  }
  [[nodiscard]] int HesitationTurn(ai::TargetApproacher& /*approacher*/,
                                   int chance, int kind, int slot) override {
    calls.push_back(Hesitate(chance, kind, slot));
    if (next_hesitation >= hesitations.size()) return otherwise;
    return hesitations.at(next_hesitation++);
  }
  void ClearWalkTests() override { calls.push_back(ClearWalk()); }
  void RefreshTargetTile(ai::TargetApproacher& /*approacher*/) override {
    calls.push_back(Refresh());
  }
};

// Actor 3 on tile (10, 10) at its centre, approaching actor 5 standing at the
// centre of tile (14, 10), which the approach last saw at half tile (29, 21).
// Nothing has changed since the last call.
ai::TargetApproacher Approacher(std::uint32_t relation,
                                std::uint32_t distance) {
  ai::TargetApproacher approacher{.tile = Tile(10, 10, kTag),
                                  .saved_destination = Tile(1, 1),
                                  .x = Centre(10),
                                  .z = Centre(10),
                                  .actor_id = kActor,
                                  .range = kRange,
                                  .previous_range = kRange,
                                  .target_id = kTarget,
                                  .previous_option = 1,
                                  .previous_target_id = kTarget,
                                  .chase_destination = Tile(2, 2, kTag),
                                  .target_half_tile_x = 29,
                                  .target_half_tile_z = 21,
                                  .movement_target = 16,
                                  .destination = Tile(14, 10, kTag)};
  approacher.relations.at(kTarget) = relation;
  approacher.distances_squared.at(kTarget) = distance;
  return approacher;
}

FakeApproachWorld World() {
  FakeApproachWorld world;
  world.models.at(kTarget) = ai::ApproachTargetModel{
      .moving = false, .x = Centre(14), .z = Centre(10)};
  return world;
}

// Runs the approach from `mode` and returns the mode it chose.
std::uint8_t ModeAfter(std::uint8_t mode, std::uint32_t distance,
                       std::uint32_t relation = kActive | kReachable) {
  auto world = World();
  auto approacher = Approacher(relation, distance);
  approacher.range_mode = mode;
  REQUIRE(ai::UpdateTargetApproachMovement(world, approacher, 1) ==
          std::optional<bool>(true));
  return approacher.range_mode;
}

int Angle(int delta_x, int delta_z) {
  return VS::Core::Math::Arctangent2(delta_x, delta_z);
}
}  // namespace

// F01748 func_800E24EC (BATTLE/BATTLE.PRG/771A8.c:1973).
TEST_CASE("Target approaches of the actor itself stand",
          "[ai][target-approach]") {
  auto world = World();
  auto approacher = Approacher(kActive, 0);
  approacher.target_id = kActor;
  approacher.previous_option = 0;
  const auto before = approacher;
  CHECK(ai::UpdateTargetApproachMovement(world, approacher, 1) ==
        std::optional<bool>(true));
  CHECK(world.calls == Calls{Command(CommandIntent::kStand), Refresh()});
  CHECK(approacher == before);
}

TEST_CASE("Target approaches refuse a target outside the slots or modelless",
          "[ai][target-approach]") {
  auto world = World();
  auto approacher = Approacher(kActive, 0);
  approacher.target_id = 16;
  CHECK(ai::UpdateTargetApproachMovement(world, approacher, 1) == std::nullopt);
  approacher.target_id = kTarget;
  world.models.at(kTarget).reset();
  CHECK(ai::UpdateTargetApproachMovement(world, approacher, 1) == std::nullopt);
  CHECK(world.calls.empty());
}

TEST_CASE("Target approaches count calls and restart after 181",
          "[ai][target-approach]") {
  const auto run = [](std::uint16_t timer) {
    auto world = World();
    auto approacher = Approacher(kActive | kReachable, kRangeBand);
    approacher.retarget_timer = timer;
    approacher.walked_close = 1;
    approacher.recover_in_range = 1;
    REQUIRE(ai::UpdateTargetApproachMovement(world, approacher, 1));
    return approacher;
  };
  const auto counted = run(180);
  CHECK(counted.retarget_timer == 181);
  CHECK(counted.walked_close == 1);
  CHECK(counted.recover_in_range == 1);
  const auto restarted = run(181);
  CHECK(restarted.retarget_timer == 0);
  CHECK(restarted.walked_close == 0);
  CHECK(restarted.recover_in_range == 0);
}

namespace {
using Fake = FakeApproachWorld;
using Approach = ai::TargetApproacher;

// Whether the approach restarts once `change` is applied to an unchanged
// approach with a counting timer.
template <typename Change>
bool Restarts(Change change, int option = 1) {
  auto world = World();
  auto approacher = Approacher(kActive, 0);
  approacher.retarget_timer = 7;
  approacher.walked_close = 1;
  change(world, approacher);
  REQUIRE(ai::UpdateTargetApproachMovement(world, approacher, option));
  return approacher.retarget_timer == 0 && approacher.walked_close == 0;
}
}  // namespace

TEST_CASE("Target approaches restart for a moved target",
          "[ai][target-approach]") {
  CHECK_FALSE(Restarts([](Fake&, Approach&) {}));
  // A standing target left its half tile.
  CHECK(Restarts([](Fake& world, Approach&) {
    world.models.at(kTarget)->x = static_cast<std::int16_t>(Centre(14) + 64);
  }));
  CHECK(Restarts([](Fake& world, Approach&) {
    world.models.at(kTarget)->z = static_cast<std::int16_t>(Centre(10) - 128);
  }));
  // A moving target is compared through the destination, in whole tiles.
  CHECK_FALSE(Restarts([](Fake& world, Approach&) {
    world.models.at(kTarget)->moving = true;
    world.models.at(kTarget)->x = 0;
  }));
  CHECK(Restarts([](Fake& world, Approach& approacher) {
    world.models.at(kTarget)->moving = true;
    approacher.destination = Tile(15, 10, kTag);
  }));
  CHECK(Restarts([](Fake& world, Approach& approacher) {
    world.models.at(kTarget)->moving = true;
    approacher.destination = Tile(14, 11, kTag);
  }));
}

TEST_CASE("Target approaches restart for a changed range, target or option",
          "[ai][target-approach]") {
  CHECK(Restarts([](Fake&, Approach& approacher) { approacher.range = 301; }));
  CHECK(Restarts(
      [](Fake&, Approach& approacher) { approacher.previous_target_id = 6; }));
  CHECK(Restarts([](Fake&, Approach&) {}, 0));
  // A flying actor always asks for a close approach.
  CHECK_FALSE(Restarts(
      [](Fake&, Approach& approacher) { approacher.flies = true; }, 0));
}

TEST_CASE("Target approaches remember the option, target and range",
          "[ai][target-approach]") {
  auto world = World();
  auto approacher = Approacher(kActive, 0);
  approacher.range = 350;
  approacher.previous_option = 0;
  approacher.previous_target_id = 9;
  REQUIRE(ai::UpdateTargetApproachMovement(world, approacher, 0x101));
  CHECK(approacher.previous_option == 1);
  CHECK(approacher.previous_target_id == kTarget);
  CHECK(approacher.previous_range == 350);
}

TEST_CASE("Target approach range modes follow the distance bands",
          "[ai][target-approach]") {
  CHECK(ModeAfter(kFar, kFarBand + 1) == kFar);
  CHECK(ModeAfter(kFar, kFarBand) == kInRange);
  CHECK(ModeAfter(kInRange, kRangeBand + 1) == kFar);
  CHECK(ModeAfter(kInRange, kRangeBand) == kInRange);
  CHECK(ModeAfter(kInRange, kCloseBand + 1) == kInRange);
  CHECK(ModeAfter(kInRange, kCloseBand) == kClose);
  CHECK(ModeAfter(kClose, kLeaveCloseBand + 1) == kInRange);
  CHECK(ModeAfter(kClose, kLeaveCloseBand) == kClose);
  CHECK(ModeAfter(3, 0) == 3);
  // Without the reachable bit the target is far.
  CHECK(ModeAfter(kClose, 0, kActive) == kFar);
}

TEST_CASE("Target approach bands square in 32 bits", "[ai][target-approach]") {
  auto world = World();
  // (65632 - 96)^2 is 2^32, which wraps to zero: any distance is far.
  auto approacher = Approacher(kActive | kReachable, 1);
  approacher.range = 65632;
  approacher.previous_range = 65632;
  REQUIRE(ai::UpdateTargetApproachMovement(world, approacher, 1));
  CHECK(approacher.range_mode == kFar);
  // A range below the margin squares its negative difference.
  approacher = Approacher(kActive | kReachable, 46 * 46);
  approacher.range = 50;
  approacher.previous_range = 50;
  REQUIRE(ai::UpdateTargetApproachMovement(world, approacher, 1));
  CHECK(approacher.range_mode == kInRange);
}

TEST_CASE("Target approaches recover after a walk or while held in range",
          "[ai][target-approach]") {
  const auto run = [](std::uint8_t mode, std::uint32_t distance,
                      std::uint8_t walked, std::uint8_t held) {
    auto world = World();
    world.opcode = 0;
    auto approacher = Approacher(kActive | kReachable, distance);
    approacher.range_mode = mode;
    approacher.walked_close = walked;
    approacher.recover_in_range = held;
    REQUIRE(ai::UpdateTargetApproachMovement(world, approacher, 1) ==
            std::optional<bool>(true));
    return world.calls;
  };
  CHECK(run(kFar, kRangeBand + 1, 1, 0) == Calls{Recover(kTarget), Refresh()});
  CHECK(run(kInRange, kRangeBand, 0, 1) == Calls{Recover(kTarget), Refresh()});
  // A close target keeps closing in after a walk, and a far one ignores the
  // hold.
  CHECK(run(kClose, 0, 1, 0).front() == ClearWalk());
  CHECK(run(kFar, kRangeBand + 1, 0, 1).front() == Dispatch(0, 0));
  // In range recovers toward the target.
  CHECK(run(kInRange, kRangeBand, 0, 0) == Calls{Recover(kTarget), Refresh()});
}

namespace {
// Approaches a far target and checks the walk toward it.
ai::TargetApproacher ApproachFar(int option, std::uint32_t distance) {
  auto world = World();
  auto approacher = Approacher(kActive | kReachable, distance);
  approacher.previous_option = static_cast<std::uint8_t>(option);
  REQUIRE(ai::UpdateTargetApproachMovement(world, approacher, option) ==
          std::optional<bool>(true));
  CHECK(world.calls == Calls{Dispatch(0, 0), Refresh()});
  CHECK(approacher.facing_locked == 1);
  CHECK(approacher.movement_target == kTarget);
  CHECK(approacher.destination == Tile(14, 10, kTag));
  return approacher;
}
}  // namespace

TEST_CASE("Target approaches run toward a far target",
          "[ai][target-approach]") {
  // Beyond the near radius it runs; option 0 asks for no close approach.
  const auto closing = ApproachFar(1, kRangeBand + 1);
  CHECK(closing.close_approach == 1);
  CHECK(closing.gait == 1);
  const auto holding = ApproachFar(0, kFarBand + 1);
  CHECK(holding.close_approach == 0);
  CHECK(holding.gait == 1);
}

TEST_CASE("Target approaches walk toward a far target within the near radius",
          "[ai][target-approach]") {
  // A range of 96 is far beyond zero.
  static_assert(ai::kNearTargetDistanceSquared == 100);
  auto world = World();
  auto approacher = Approacher(kActive | kReachable, 100);
  approacher.range = 96;
  approacher.previous_range = 96;
  REQUIRE(ai::UpdateTargetApproachMovement(world, approacher, 1));
  CHECK(approacher.range_mode == kFar);
  CHECK(approacher.gait == 0);
  approacher.distances_squared.at(kTarget) = 101;
  REQUIRE(ai::UpdateTargetApproachMovement(world, approacher, 1));
  CHECK(approacher.gait == 1);
}

namespace {
// Holds the default distance from a target two tiles ahead along +x, 256 units
// from the actor, with the projection landing on `projected`.
std::pair<Calls, ai::TargetApproacher> HoldDistance(std::int32_t range,
                                                    std::uint32_t projected) {
  auto world = World();
  world.projected = projected;
  auto approacher = Approacher(kActive | kReachable, 0xFFFFFFFF);
  approacher.range = range;
  approacher.previous_range = range;
  approacher.movement_flags = kHoldDistance;
  approacher.destination = Tile(12, 10, kTag);
  REQUIRE(ai::UpdateTargetApproachMovement(world, approacher, 1));
  CHECK(approacher.saved_destination == Tile(12, 10, kTag));
  return {world.calls, approacher};
}

int HoldAngle() { return Angle(-256, 0) & 0xFFF; }
}  // namespace

TEST_CASE("Target approaches holding distance project back by the range",
          "[ai][target-approach]") {
  // 256 less |300 - 96|.
  auto [calls, approacher] = HoldDistance(kRange, Tile(11, 10));
  CHECK(calls == Calls{Project(Tile(10, 10, kTag), HoldAngle(), 52),
                       Dispatch(0, 0), Refresh()});
  CHECK(approacher.destination == Tile(11, 10));
  CHECK(approacher.approach_directly == 0);
  // A range below 96 subtracts its magnitude.
  std::tie(calls, approacher) = HoldDistance(50, Tile(11, 10));
  CHECK(calls.front() == Project(Tile(10, 10, kTag), HoldAngle(), 256 - 46));
  // A range beyond the distance wraps the unsigned distance.
  std::tie(calls, approacher) = HoldDistance(1000, Tile(11, 10));
  CHECK(calls.front() ==
        Project(Tile(10, 10, kTag), HoldAngle(), std::uint32_t{256} - 904U));
}

TEST_CASE("Target approaches projected onto their own tile walk straight on",
          "[ai][target-approach]") {
  // The walk goes straight to the original destination.
  const auto approacher = HoldDistance(kRange, Tile(10, 10, 0x12000000)).second;
  CHECK(approacher.approach_directly == 1);
  CHECK(approacher.destination == Tile(12, 10, kTag));
}

namespace {
// Closes in on a close target at (1900, 1000) and checks the step toward it.
ai::TargetApproacher CloseIn(std::uint8_t opcode, std::uint8_t to_point) {
  auto world = World();
  world.opcode = opcode;
  world.models.at(kTarget) =
      ai::ApproachTargetModel{.moving = false, .x = 1900, .z = 1000};
  auto approacher = Approacher(kActive | kReachable, kCloseBand);
  approacher.range_mode = kClose;
  approacher.approaches_target_point = to_point;
  REQUIRE(ai::UpdateTargetApproachMovement(world, approacher, 1) ==
          std::optional<bool>(true));
  CHECK(world.calls ==
        Calls{ClearWalk(),
              Dispatch(1, Angle(1900 - Centre(10), 1000 - Centre(10))),
              Refresh()});
  CHECK(approacher.height_filter == 1);
  CHECK(approacher.near_target_detour == 1);
  CHECK(approacher.move_x == 1900);
  CHECK(approacher.move_z == 1000);
  return approacher;
}
}  // namespace

TEST_CASE("Target approaches close in on a close target",
          "[ai][target-approach]") {
  const auto walked = CloseIn(9, 0);
  CHECK(walked.walked_close == 1);
  CHECK(walked.movement_target == kTarget);
  CHECK(walked.gait == 0);
  const auto stood = CloseIn(8, 0);
  CHECK(stood.walked_close == 0);
}

TEST_CASE("Target approaches close in toward the target point",
          "[ai][target-approach]") {
  const auto to_point = CloseIn(10, 1);
  CHECK(to_point.movement_target == 16);
  CHECK(to_point.gait == 1);
  CHECK(to_point.toward_point == 1);
  CHECK(to_point.target_x == 1900);
  CHECK(to_point.target_z == 1000);
}

TEST_CASE("Target approaches close in only for a close approach",
          "[ai][target-approach]") {
  auto world = World();
  auto approacher = Approacher(kActive | kReachable, kCloseBand);
  approacher.range_mode = kClose;
  approacher.previous_option = 0;
  REQUIRE(ai::UpdateTargetApproachMovement(world, approacher, 0));
  CHECK(approacher.range_mode == kInRange);
  CHECK(world.calls == Calls{Recover(kTarget), Refresh()});
}

TEST_CASE("Target approaches refuse closing in without a battle actor",
          "[ai][target-approach]") {
  auto world = World();
  world.opcode.reset();
  auto approacher = Approacher(kActive | kReachable, kCloseBand);
  approacher.range_mode = kClose;
  CHECK(ai::UpdateTargetApproachMovement(world, approacher, 1) == std::nullopt);
}

TEST_CASE("Target approaches look for a target just lost",
          "[ai][target-approach]") {
  const auto run = [](std::uint32_t relation, int hesitating) {
    auto world = World();
    world.otherwise = hesitating;
    auto approacher = Approacher(relation, 0);
    const auto result = ai::UpdateTargetApproachMovement(world, approacher, 1);
    CHECK(approacher.chasing_lost_target == 0);
    return std::pair{result, world.calls};
  };
  CHECK(run(kWasActive | kVisible, 1) ==
        std::pair{std::optional<bool>(false), Calls{Refresh()}});
  CHECK(run(kWasActive, 1) ==
        std::pair{std::optional<bool>(true),
                  Calls{Awareness(7, 40), Hesitate(128, 1, 1), Refresh()}});
  CHECK(run(kWasActive, 0).first == std::optional<bool>(false));
  // A target never active waits.
  CHECK(run(0, 1) == std::pair{std::optional<bool>(true),
                               Calls{Hesitate(0, 3, 1), Refresh()}});
  CHECK(run(0, 0).first == std::optional<bool>(false));
}

namespace {
// Starts chasing a target just lost from sight.
std::pair<Calls, ai::TargetApproacher> ChaseLostTarget() {
  auto world = World();
  auto approacher = Approacher(kWasActive | kVisible, 0);
  approacher.chases_lost_target = 1;
  REQUIRE(ai::UpdateTargetApproachMovement(world, approacher, 1) ==
          std::optional<bool>(true));
  return {world.calls, approacher};
}
}  // namespace

TEST_CASE("Target approaches chase a lost target to its last tile",
          "[ai][target-approach]") {
  const auto approacher = ChaseLostTarget().second;
  CHECK(approacher.chasing_lost_target == 1);
  CHECK(approacher.chase_timeout == 254);
  // Half tile (29, 21) is tile (14, 10); the chase keeps its tag bytes.
  CHECK(approacher.chase_destination == Tile(14, 10, kTag));
  CHECK(approacher.destination == Tile(14, 10, kTag));
}

TEST_CASE("Target approach chases run until the target is active",
          "[ai][target-approach]") {
  auto [calls, approacher] = ChaseLostTarget();
  CHECK(approacher.gait == 1);
  CHECK(approacher.facing_locked == 1);
  CHECK(calls == Calls{Dispatch(0, 0), Refresh()});
  auto world = World();
  approacher.relations.at(kTarget) = kActive;
  REQUIRE(ai::UpdateTargetApproachMovement(world, approacher, 1));
  CHECK(approacher.chasing_lost_target == 0);
}

TEST_CASE("Target approach chases end at the tile or the timeout",
          "[ai][target-approach]") {
  const auto run = [](std::uint32_t relation, std::uint32_t destination,
                      std::uint8_t timeout, int hesitating) {
    auto world = World();
    world.otherwise = hesitating;
    auto approacher = Approacher(relation, 0);
    approacher.chasing_lost_target = 1;
    approacher.chase_timeout = timeout;
    approacher.chase_destination = destination;
    const auto result = ai::UpdateTargetApproachMovement(world, approacher, 1);
    return std::tuple{result, approacher.chasing_lost_target,
                      approacher.chase_timeout, world.calls};
  };
  const std::uint32_t here = Tile(10, 10, 0x77000000);
  const std::uint32_t there = Tile(12, 10);
  // Arrived with the target in sight: stop, still chasing.
  CHECK(run(kVisible, here, 9, 0) ==
        std::tuple{std::optional<bool>(false), std::uint8_t{1}, std::uint8_t{8},
                   Calls{Refresh()}});
  // Arrived without it: look around, and give up once done hesitating.
  CHECK(run(0, here, 9, 1) ==
        std::tuple{std::optional<bool>(true), std::uint8_t{1}, std::uint8_t{8},
                   Calls{Awareness(7, 40), Hesitate(128, 1, 1), Refresh()}});
  CHECK(run(0, here, 9, 0) ==
        std::tuple{std::optional<bool>(false), std::uint8_t{0}, std::uint8_t{8},
                   Calls{Awareness(7, 40), Hesitate(128, 1, 1), Refresh()}});
  // The last call of the timeout gives up.
  CHECK(run(0, there, 1, 0) == std::tuple{std::optional<bool>(false),
                                          std::uint8_t{0}, std::uint8_t{0},
                                          Calls{Refresh()}});
  CHECK(run(0, there, 0, 0) == std::tuple{std::optional<bool>(false),
                                          std::uint8_t{0}, std::uint8_t{0},
                                          Calls{Refresh()}});
}

TEST_CASE("Target approaches recover a lost target after a walk",
          "[ai][target-approach]") {
  auto world = World();
  auto approacher = Approacher(0, 0);
  approacher.walked_close = 1;
  approacher.chasing_lost_target = 1;
  REQUIRE(ai::UpdateTargetApproachMovement(world, approacher, 1) ==
          std::optional<bool>(true));
  CHECK(world.calls == Calls{Recover(kTarget), Refresh()});
  CHECK(approacher.chase_timeout == 0);
}

// F01749 func_800E2B2C (BATTLE/BATTLE.PRG/771A8.c:2185).
namespace {
// Facing actor 5 two tiles along +x after the target's tile changed.
ai::TargetApproacher Facer(std::uint32_t relation) {
  auto facer = Approacher(relation, 0);
  facer.facing_delay = 12;
  facer.move_x = Centre(12);
  facer.move_z = Centre(10);
  facer.previous_facing_tile = Tile(13, 10);
  return facer;
}

int FacingAngle() { return Angle(Centre(10) - Centre(12), 0); }
}  // namespace

TEST_CASE("Target facing of the actor itself stands", "[ai][target-facing]") {
  auto world = World();
  auto facer = Facer(kVisible);
  CHECK(ai::UpdateTargetFacing(world, facer, kActor) ==
        std::optional<bool>(true));
  CHECK(world.calls == Calls{Command(CommandIntent::kStand)});
}

TEST_CASE("Target facing waits without a delay or on a reaction",
          "[ai][target-facing]") {
  auto world = World();
  auto facer = Facer(kVisible);
  facer.facing_delay = 0;
  CHECK(ai::UpdateTargetFacing(world, facer, kTarget) ==
        std::optional<bool>(false));
  CHECK(world.calls.empty());

  facer = Facer(kVisible);
  facer.reaction_pending = 1;
  facer.facing_pending = 1;
  world.hesitations = {1, 0};
  CHECK(ai::UpdateTargetFacing(world, facer, kTarget) ==
        std::optional<bool>(true));
  CHECK(facer.facing_pending == 1);
  CHECK(ai::UpdateTargetFacing(world, facer, kTarget) ==
        std::optional<bool>(false));
  CHECK(facer.facing_pending == 0);
  CHECK(world.calls == Calls{Hesitate(128, 3, 6), Hesitate(128, 3, 6)});
}

TEST_CASE("Target facing refuses a target outside the slots",
          "[ai][target-facing]") {
  auto world = World();
  auto facer = Facer(kVisible);
  CHECK(ai::UpdateTargetFacing(world, facer, 16) == std::nullopt);
  CHECK(ai::UpdateTargetFacing(world, facer, -1) == std::nullopt);
  CHECK(world.calls.empty());
}

TEST_CASE("Target facing turns toward a visible target that moved",
          "[ai][target-facing]") {
  auto world = World();
  auto facer = Facer(kVisible);
  REQUIRE(ai::UpdateTargetFacing(world, facer, kTarget) ==
          std::optional<bool>(true));
  CHECK(facer.facing_pending == 1);
  CHECK(facer.angle == static_cast<std::int16_t>(FacingAngle()));
  CHECK(facer.previous_facing_tile == facer.destination);
  CHECK(facer.turn_timer == 12);
  CHECK(world.calls == Calls{Command(CommandIntent::kTurn)});
}

TEST_CASE("Target facing keeps a pending angle and ignores an unmoved target",
          "[ai][target-facing]") {
  auto world = World();
  auto facer = Facer(kVisible);
  REQUIRE(ai::UpdateTargetFacing(world, facer, kTarget));
  // A pending turn keeps its angle; with none, nothing happens.
  facer.destination = Tile(15, 10);
  facer.angle = 0x123;
  REQUIRE(ai::UpdateTargetFacing(world, facer, kTarget));
  CHECK(facer.angle == 0x123);
  facer = Facer(0);
  CHECK(ai::UpdateTargetFacing(world, facer, kTarget) ==
        std::optional<bool>(false));
  facer = Facer(kVisible);
  facer.previous_facing_tile = Tile(14, 10, 0x33003300);
  CHECK(ai::UpdateTargetFacing(world, facer, kTarget) ==
        std::optional<bool>(false));
}

namespace {
// Updates a pending turn that already faces the angle.
std::tuple<std::optional<bool>, ai::TargetApproacher, Calls> FaceAgain(
    std::uint32_t relation, int hesitating) {
  auto world = World();
  world.otherwise = hesitating;
  auto facer = Facer(relation);
  facer.previous_facing_tile = facer.destination;
  facer.facing_pending = 1;
  // Facing 0xF00 turned by 0x200 is 0x100, which matches -0xF00.
  facer.model_facing = 0x0F00;
  facer.model_look_yaw = 0x0200;
  facer.angle = -0x0F00;
  const auto result = ai::UpdateTargetFacing(world, facer, kTarget);
  return {result, facer, world.calls};
}
}  // namespace

TEST_CASE("Target facing re-aims at a visible target once facing the angle",
          "[ai][target-facing]") {
  const auto [turned, aimed, calls] = FaceAgain(kVisible, 0);
  CHECK(turned == std::optional<bool>(true));
  CHECK(aimed.facing_pending == 1);
  CHECK(aimed.angle == static_cast<std::int16_t>(FacingAngle() & 0xFFF));
  CHECK(calls == Calls{Command(CommandIntent::kTurn)});
}

TEST_CASE("Target facing hesitates or stops once facing the angle",
          "[ai][target-facing]") {
  const auto [waiting, waited, wait_calls] = FaceAgain(0, 1);
  CHECK(waiting == std::optional<bool>(true));
  CHECK(waited.facing_pending == 1);
  CHECK(wait_calls == Calls{Hesitate(128, 1, 6)});

  const auto [done, stopped, done_calls] = FaceAgain(0, 0);
  CHECK(done == std::optional<bool>(false));
  CHECK(stopped.facing_pending == 0);
  CHECK(stopped.turn_timer == 12);
}

TEST_CASE("Target approach keeps its original module entries",
          "[ai][target-approach][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [symbols](unsigned address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  CHECK(resolve(0x800E24ECU) ==
        reinterpret_cast<void*>(&ai::UpdateLiveTargetApproachMovement));
  CHECK(resolve(0x800E2B2CU) ==
        reinterpret_cast<void*>(&ai::UpdateLiveTargetFacing));
}
