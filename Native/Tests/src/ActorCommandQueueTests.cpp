#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorCommands.hpp"
#include "Actors/ActorCommandsLive.hpp"
#include "Content/ModuleData.h"
#include "Core/Math/ScalarMath.hpp"

extern "C" {
#include "Actors/ActorCommandsLiveState.h"
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Field/Interactions.h"
}

namespace actors = VS::Actors;
using actors::ActorCommand;
using actors::AshleyMoveInput;
using actors::CommandArgs;
using actors::CommandJump;
using actors::CommandPlacement;
using actors::MotionVector;
using actors::PackedTile;
using actors::TravelPose;

namespace {
constexpr int kSlots = actors::kCommandActorSlots;
constexpr std::uint8_t kNoTarget = actors::kCommandNoTarget;

int FacingAlong(int dx, int dz) {
  return VS::Core::Math::Arctangent2(dx, dz) + 0x800;
}

// Two halfwords packed as one little-endian argument word.
constexpr std::int32_t Halves(int low, int high) {
  return static_cast<std::int32_t>(
      (static_cast<std::uint32_t>(static_cast<std::uint16_t>(high)) << 16) |
      static_cast<std::uint16_t>(low));
}

// Four bytes packed as one little-endian argument word.
constexpr std::int32_t Bytes(int b0, int b1, int b2, int b3) {
  return static_cast<std::int32_t>(
      (static_cast<std::uint32_t>(b3 & 0xFF) << 24) |
      (static_cast<std::uint32_t>(b2 & 0xFF) << 16) |
      (static_cast<std::uint32_t>(b1 & 0xFF) << 8) |
      static_cast<std::uint32_t>(b0 & 0xFF));
}

ActorCommand Queued(std::uint8_t opcode, CommandArgs args = {},
                    std::uint8_t target = kNoTarget, std::uint8_t flags = 0) {
  return {.state = actors::kCommandQueued,
          .opcode = opcode,
          .target = target,
          .flags = flags,
          .args = args};
}

ActorCommand Waiting(std::uint8_t opcode, CommandArgs args = {}) {
  ActorCommand command = Queued(opcode, args);
  command.state = actors::kCommandWaiting;
  return command;
}

struct HeadTrack {
  int actor_id = 0;
  int target = 0;
  std::optional<MotionVector> aim;
  int step = 0;
  bool operator==(const HeadTrack&) const = default;
};

struct Navigation {
  int actor_id = 0;
  int direction = 0;
  int distance = 0;
  int facing = 0;
  bool operator==(const Navigation&) const = default;
};

struct Turn {
  int actor_id = 0;
  int facing = 0;
  int speed = 0;
  bool operator==(const Turn&) const = default;
};

struct Jump {
  int actor_id = 0;
  CommandJump jump = CommandJump::kTile;
  PackedTile target{};
  int speed = 0;
  int facing = 0;
  bool operator==(const Jump&) const = default;
};

struct Teleport {
  int actor_id = 0;
  PackedTile target{};
  int facing = 0;
  bool operator==(const Teleport&) const = default;
};

struct Travel {
  int actor_id = 0;
  TravelPose pose = TravelPose::kTile;
  MotionVector target{};
  int facing = 0;
  bool operator==(const Travel&) const = default;
};

struct Move {
  int actor_id = 0;
  MotionVector step{};
  int speed = 0;
  int facing = 0;
  bool operator==(const Move&) const = default;
};

class FakeCommandHost final : public actors::ActorCommandHost {
 public:
  std::array<std::optional<ActorCommand>, kSlots> commands{};
  std::array<unsigned, kSlots> kinds{};
  std::array<std::uint32_t, kSlots> state_flags{};
  std::array<CommandPlacement, kSlots> placements{};
  actors::CommandActorGait gait{
      .walk_speed = 3, .gait_class = 0, .run_speed = 7};
  int control_mode = 0;
  int trap_result = 0;
  std::optional<std::uint16_t> limb_action;
  bool preview_missing = false;

  std::vector<std::string> calls;
  std::vector<int> placement_queries;
  std::vector<int> unknown_opcodes;
  std::vector<int> updated;
  std::vector<std::pair<int, CommandPlacement>> trap_checks;
  std::vector<std::pair<int, int>> sounds;
  std::vector<HeadTrack> head;
  std::vector<Turn> turns;
  std::vector<Navigation> navigation;
  std::vector<Jump> jumps;
  std::vector<Teleport> teleports;
  std::vector<Travel> travels;
  std::vector<Move> ground;
  std::vector<Move> vectors;
  std::vector<Move> flights;
  std::vector<std::pair<int, int>> combat_poses;
  mutable std::vector<std::pair<unsigned, unsigned>> limb_queries;
  std::vector<std::pair<int, std::uint16_t>> selections;

  void Queue(int actor_id, const ActorCommand& command, unsigned kind = 2) {
    commands.at(actor_id) = command;
    kinds.at(actor_id) = kind;
  }
  // The queued command of `actor_id`; the test queued one.
  [[nodiscard]] ActorCommand& At(int actor_id) {
    std::optional<ActorCommand>& command = commands.at(actor_id);
    if (!command.has_value()) {
      throw std::logic_error("no command queued for the actor");
    }
    return *command;
  }
  [[nodiscard]] const ActorCommand& At(int actor_id) const {
    const std::optional<ActorCommand>& command = commands.at(actor_id);
    if (!command.has_value()) {
      throw std::logic_error("no command queued for the actor");
    }
    return *command;
  }
  [[nodiscard]] std::uint8_t State(int actor_id) const {
    return At(actor_id).state;
  }

  [[nodiscard]] std::optional<ActorCommand> Command(
      int actor_id) const override {
    return commands.at(actor_id);
  }
  void SetCommandState(int actor_id, std::uint8_t state) override {
    calls.emplace_back("state");
    At(actor_id).state = state;
  }
  void SetCommandFlags(int actor_id, std::uint8_t flags) override {
    calls.emplace_back("flags");
    At(actor_id).flags = flags;
  }
  void SetCommandArgs(int actor_id, const CommandArgs& args) override {
    calls.emplace_back("args");
    At(actor_id).args = args;
  }
  [[nodiscard]] unsigned Kind(int actor_id) const override {
    return kinds.at(actor_id);
  }
  [[nodiscard]] actors::CommandActorGait Gait(int) const override {
    return gait;
  }
  [[nodiscard]] std::uint32_t StateFlags(int actor_id) const override {
    return state_flags.at(actor_id);
  }
  [[nodiscard]] int ControlMode() const override { return control_mode; }
  [[nodiscard]] CommandPlacement Placement(int actor_id) override {
    calls.emplace_back("placement");
    placement_queries.push_back(actor_id);
    return placements.at(actor_id);
  }
  void ReportUnknownWaitingOpcode(int, int opcode) override {
    calls.emplace_back("unknown");
    unknown_opcodes.push_back(opcode);
  }
  void UpdateMovement(int actor_id) override {
    calls.emplace_back("update");
    updated.push_back(actor_id);
  }
  int CheckTrapAndFallTriggers(int actor_id,
                               const CommandPlacement& placement) override {
    calls.emplace_back("trap");
    trap_checks.emplace_back(actor_id, placement);
    return trap_result;
  }
  void PlayModelSound(int actor_id, int sound) override {
    calls.emplace_back("sound");
    sounds.emplace_back(actor_id, sound);
  }
  void TintRed(int) override { calls.emplace_back("tint-red"); }
  void RestoreTint() override { calls.emplace_back("tint-restore"); }
  void RestoreNeutralPose(int) override { calls.emplace_back("neutral"); }
  void RestoreIdlePose(int) override { calls.emplace_back("idle"); }
  void SetHeadTracking(int actor_id, int target,
                       const std::optional<MotionVector>& aim,
                       int step) override {
    calls.emplace_back("head");
    head.push_back({actor_id, target, aim, step});
  }
  void TurnNavigation(int actor_id, int facing) override {
    calls.emplace_back("turn");
    turns.push_back({actor_id, facing, 0});
  }
  void TurnAtSpeed(int actor_id, std::int16_t facing, int speed) override {
    calls.emplace_back("turn-at-speed");
    turns.push_back({actor_id, facing, speed});
  }
  void QueueNavigationTarget(int actor_id, int direction, int distance,
                             int facing) override {
    calls.emplace_back("navigation");
    navigation.push_back({actor_id, direction, distance, facing});
  }
  void BeginJump(int actor_id, CommandJump jump, PackedTile target, int speed,
                 int facing) override {
    calls.emplace_back("jump");
    jumps.push_back({actor_id, jump, target, speed, facing});
  }
  void BeginTeleport(int actor_id, PackedTile target, int facing) override {
    calls.emplace_back("teleport");
    teleports.push_back({actor_id, target, facing});
  }
  void BeginTravel(int actor_id, TravelPose pose, const MotionVector& target,
                   int facing) override {
    calls.emplace_back("travel");
    travels.push_back({actor_id, pose, target, facing});
  }
  void QueueGroundOffset(int actor_id, const MotionVector& step) override {
    calls.emplace_back("ground");
    ground.push_back({actor_id, step, 0, 0});
  }
  void QueueMovementVector(int actor_id, const MotionVector& step,
                           int facing) override {
    calls.emplace_back("vector");
    vectors.push_back({actor_id, step, 0, facing});
  }
  void QueueFlightMovement(int actor_id, const MotionVector& step, int speed,
                           int facing) override {
    calls.emplace_back("flight");
    flights.push_back({actor_id, step, speed, facing});
  }
  void FinishCombatPose(int actor_id, std::int16_t facing) override {
    calls.emplace_back("combat-pose");
    combat_poses.emplace_back(actor_id, facing);
  }
  [[nodiscard]] std::optional<std::uint16_t> LimbActionId(
      int, unsigned limb, unsigned slot) const override {
    limb_queries.emplace_back(limb, slot);
    return limb_action;
  }
  bool BeginActionSelection(int actor_id, std::uint16_t action) override {
    calls.emplace_back("select");
    selections.emplace_back(actor_id, action);
    return preview_missing;
  }
};

// Runs one queued command for actor 0 and returns its new state.
std::uint8_t RunOne(FakeCommandHost& host, const ActorCommand& command) {
  host.Queue(0, command);
  CHECK(actors::ExecuteQueuedActorCommands(host) == 0);
  return host.State(0);
}

// The steps of the queued moves, in order.
std::vector<MotionVector> StepsOf(const std::vector<Move>& moves) {
  std::vector<MotionVector> steps;
  steps.reserve(moves.size());
  for (const Move& move : moves) {
    steps.push_back(move.step);
  }
  return steps;
}

// Actor 1 is a field-only kind, actor 2 is animating, actor 3 is a battle
// kind and actor 4's command has finished.
FakeCommandHost MixedKindQueue() {
  FakeCommandHost host;
  host.Queue(1, Queued(1), 1);
  host.Queue(2, Queued(1));
  host.state_flags[2] = actors::kStateActionAnimating;
  host.Queue(3, Queued(1), 4);
  ActorCommand finished = Queued(1);
  finished.state = 0x20;
  host.Queue(4, finished);
  return host;
}

// Actor 0 is at (100, 0, 100) and actor 5 at (400, 0, -200).
FakeCommandHost NavigationHost() {
  FakeCommandHost host;
  host.placements[0].position = {.x = 100, .y = 0, .z = 100, .w = 0};
  host.placements[5].position = {.x = 400, .y = 0, .z = -200, .w = 0};
  return host;
}

// Actor 0 is at (100, -50, 200) and actor 5 at (300, 0, 100).
FakeCommandHost MoveHost() {
  FakeCommandHost host;
  host.placements[0].position = {.x = 100, .y = -50, .z = 200, .w = 0};
  host.placements[5].position = {.x = 300, .y = 0, .z = 100, .w = 0};
  return host;
}
}  // namespace

TEST_CASE("Actor command arguments read as little-endian halves",
          "[actors][actor-command-queue]") {
  ActorCommand command;
  command.args = {Halves(0x7FFE, -32767), Bytes(0x01, 0x02, 0x03, 0x04), 9};
  CHECK(command.Half(0) == 0x7FFE);
  CHECK(command.Half(1) == -32767);
  CHECK(command.Half(2) == 0x0201);
  CHECK(command.Half(3) == 0x0403);
  CHECK(command.Vector() ==
        MotionVector{.x = 0x7FFE, .y = -32767, .z = 0x0201, .w = 0x0403});
}

TEST_CASE("Actor command arguments read as little-endian bytes",
          "[actors][actor-command-queue]") {
  ActorCommand command;
  command.args = {Halves(0x7FFE, -32767), Bytes(0x01, 0x02, 0x03, 0x04), 9};
  CHECK(command.Byte(0) == 0xFE);
  CHECK(command.Byte(1) == 0x7F);
  CHECK(command.Byte(2) == 0x01);
  CHECK(command.Byte(3) == 0x80);
  CHECK(command.Byte(4) == 0x01);
  CHECK(command.Byte(7) == 0x04);
  CHECK(command.Tile() ==
        PackedTile{.x = 0xFE, .layer = 0x7F, .z = 0x01, .spare = 0x80});
}

TEST_CASE("Consuming a command offset takes whole Q12 steps or 8-unit chunks",
          "[actors][actor-command-queue]") {
  ActorCommand command;
  command.args = {0x9000, -0x9000, 0x7FFF};
  CHECK(actors::ConsumeActorMovementOffset(command) ==
        MotionVector{.x = 8, .y = 7, .z = -8, .w = 0});
  CHECK(command.args == CommandArgs{0x1000, -0x1000, 0});
  CHECK(actors::ConsumeActorMovementOffset(command) ==
        MotionVector{.x = 1, .y = 0, .z = -1, .w = 0});
  CHECK(command.args == CommandArgs{});

  // -0x7FFF is still whole; -0x8000 is one chunk; division truncates.
  command.args = {-0x7FFF, -0x8000, -0x0FFF};
  CHECK(actors::ConsumeActorMovementOffset(command) ==
        MotionVector{.x = -7, .y = 0, .z = -8, .w = 0});
  CHECK(command.args == CommandArgs{0, 0, 0});
}

TEST_CASE("The command executor skips empty, animating and field-only actors",
          "[actors][actor-command-queue]") {
  FakeCommandHost host = MixedKindQueue();
  CHECK(actors::ExecuteQueuedActorCommands(host) == 0);
  CHECK(host.State(1) == actors::kCommandQueued);
  CHECK(host.State(2) == actors::kCommandQueued);
  CHECK(host.State(3) == actors::kCommandIdle);
  CHECK(host.State(4) == 0x20);
  CHECK(host.placement_queries == std::vector<int>{3});
  REQUIRE(host.trap_checks.size() == 1);
  CHECK(host.trap_checks[0].first == 3);
  CHECK(host.updated.empty());
}

TEST_CASE(
    "In the field movement control mode every command kind runs and moves",
    "[actors][actor-command-queue]") {
  FakeCommandHost host = MixedKindQueue();
  host.control_mode = actors::kFieldMovementControlMode;
  CHECK(actors::ExecuteQueuedActorCommands(host) == 0);
  CHECK(host.State(1) == actors::kCommandIdle);
  CHECK(host.State(2) == actors::kCommandQueued);
  CHECK(host.State(3) == actors::kCommandIdle);
  CHECK(host.updated == std::vector<int>{1, 3});
  CHECK(host.trap_checks.empty());
  CHECK(host.calls == std::vector<std::string>{"placement", "flags", "state",
                                               "update", "placement", "flags",
                                               "state", "update"});
}

TEST_CASE("Waiting movement commands finish when the actor stops",
          "[actors][actor-command-queue]") {
  const std::uint8_t opcode =
      GENERATE(as<std::uint8_t>{}, 9, 10, 11, 12, 14, 15, 18);
  FakeCommandHost host;
  host.Queue(0, Waiting(opcode));
  host.state_flags[0] = actors::kStateMovementActive;
  CHECK(actors::ExecuteQueuedActorCommands(host) == 0);
  CHECK(host.State(0) == actors::kCommandWaiting);

  host.state_flags[0] = 0;
  CHECK(actors::ExecuteQueuedActorCommands(host) == 0);
  CHECK(host.State(0) == actors::kCommandIdle);
  CHECK(host.placement_queries.empty());
  CHECK(host.trap_checks.empty());
}

TEST_CASE("Waiting travel and teleport commands wait on their pose flags",
          "[actors][actor-command-queue]") {
  const std::uint8_t opcode = GENERATE(as<std::uint8_t>{}, 20, 21, 22);
  FakeCommandHost host;
  host.Queue(0, Waiting(opcode));
  host.state_flags[0] = actors::kStateTravelPose;
  CHECK(actors::ExecuteQueuedActorCommands(host) == 0);
  CHECK(host.State(0) == actors::kCommandWaiting);
  host.state_flags[0] = actors::kStateMovementActive;
  CHECK(actors::ExecuteQueuedActorCommands(host) == 0);
  CHECK(host.State(0) == actors::kCommandIdle);

  for (const std::uint32_t reacting : {0x02000000U, 0x10000000U}) {
    host.Queue(0, Waiting(23));
    host.state_flags[0] = reacting;
    CHECK(actors::ExecuteQueuedActorCommands(host) == 0);
    CHECK(host.State(0) == actors::kCommandWaiting);
  }
  host.state_flags[0] = actors::kStateTravelPose;
  CHECK(actors::ExecuteQueuedActorCommands(host) == 0);
  CHECK(host.State(0) == actors::kCommandIdle);
}

TEST_CASE("A waiting ground offset steps until x and z are spent",
          "[actors][actor-command-queue]") {
  FakeCommandHost host;
  host.control_mode = actors::kFieldMovementControlMode;
  host.Queue(0, Waiting(13, {0x9000, 0x2000, 0x19000}));
  CHECK(actors::ExecuteQueuedActorCommands(host) == 0);
  CHECK(host.State(0) == actors::kCommandWaiting);
  CHECK(host.At(0).args == CommandArgs{0x1000, 0, 0x11000});
  CHECK(host.calls ==
        std::vector<std::string>{"args", "ground", "state", "update"});

  // The y remainder is not tested: the command ends with it still queued.
  CHECK(actors::ExecuteQueuedActorCommands(host) == 0);
  CHECK(host.State(0) == actors::kCommandIdle);
  CHECK(host.At(0).args == CommandArgs{0, 0, 0x9000});
  CHECK(StepsOf(host.ground) ==
        std::vector<MotionVector>{{.x = 8, .y = 8, .z = 2, .w = 0},
                                  {.x = 1, .y = 8, .z = 0, .w = 0}});
}

TEST_CASE("An unknown waiting opcode is reported and keeps waiting",
          "[actors][actor-command-queue]") {
  FakeCommandHost host;
  host.control_mode = actors::kFieldMovementControlMode;
  host.Queue(0, Waiting(2));
  CHECK(actors::ExecuteQueuedActorCommands(host) == 0);
  CHECK(host.State(0) == actors::kCommandWaiting);
  CHECK(host.unknown_opcodes == std::vector<int>{2});
  CHECK(host.calls == std::vector<std::string>{"unknown", "state", "update"});
}

TEST_CASE("A queued command plays its sound, tints red and checks traps",
          "[actors][actor-command-queue]") {
  FakeCommandHost host;
  host.placements[0].position = {.x = 10, .y = 20, .z = 30, .w = 40};
  host.Queue(0, Queued(1, {}, kNoTarget, 0x77));
  host.Queue(1, Queued(1, {}, kNoTarget, 0x0A));
  host.trap_result = 5;

  CHECK(actors::ExecuteQueuedActorCommands(host) == 5);
  CHECK(host.calls == std::vector<std::string>{"placement", "sound", "tint-red",
                                               "flags", "state", "trap"});
  CHECK(host.sounds == std::vector<std::pair<int, int>>{{0, 3}});
  CHECK(host.At(0).flags == 0x73);
  REQUIRE(host.trap_checks.size() == 1);
  CHECK(host.trap_checks[0].second == host.placements[0]);
  // The trap result ended the pass before actor 1.
  CHECK(host.State(1) == actors::kCommandQueued);
}

TEST_CASE("A queued command can restore the tint before its trap check",
          "[actors][actor-command-queue]") {
  FakeCommandHost host;
  host.Queue(1, Queued(1, {}, kNoTarget, 0x0A));
  CHECK(actors::ExecuteQueuedActorCommands(host) == 0);
  CHECK(host.calls == std::vector<std::string>{"placement", "sound",
                                               "tint-restore", "flags", "state",
                                               "trap"});
  CHECK(host.sounds == std::vector<std::pair<int, int>>{{1, 2}});
  CHECK(host.At(1).flags == 0x02);
  CHECK(host.State(1) == actors::kCommandIdle);
}

TEST_CASE("Queued pose commands finish at once",
          "[actors][actor-command-queue]") {
  FakeCommandHost host;
  for (const std::uint8_t opcode : std::to_array<std::uint8_t>({0, 1, 5, 25})) {
    CHECK(RunOne(host, Queued(opcode)) == actors::kCommandIdle);
  }
  CHECK(host.head.empty());

  host.calls.clear();
  CHECK(RunOne(host, Queued(2)) == actors::kCommandIdle);
  CHECK(RunOne(host, Queued(2, {}, 3)) == actors::kCommandIdle);
  CHECK(std::ranges::count(host.calls, "neutral") == 1);
  CHECK(std::ranges::count(host.calls, "idle") == 1);
}

TEST_CASE("Queued turn commands track the head and turn",
          "[actors][actor-command-queue]") {
  FakeCommandHost host;
  CHECK(RunOne(host, Queued(3, {Halves(-2048, 77)})) == actors::kCommandIdle);
  CHECK(host.head.back() == HeadTrack{0, -4, std::nullopt, 0});
  CHECK(host.turns.back() == Turn{0, -2048, 0});

  CHECK(RunOne(host, Queued(4, {Halves(0x0300, 0x0123)})) ==
        actors::kCommandIdle);
  CHECK(host.head.back() == HeadTrack{0, -3, MotionVector{.y = 0x0300}, -1});
  CHECK(host.turns.back() == Turn{0, 0x0300, 0x0123});
}

TEST_CASE("Queued navigation to a tile waits while the actor moves",
          "[actors][actor-command-queue]") {
  FakeCommandHost host = NavigationHost();
  const int to_target = FacingAlong(300, -300);
  const std::int32_t args = Halves(0x0123, 6);

  host.state_flags[0] = actors::kStateMovementActive;
  CHECK(RunOne(host, Queued(15, {args})) == actors::kCommandWaiting);
  CHECK(host.navigation.back() == Navigation{0, 0x0123, 6 * 4096, 0x0123});
  CHECK(host.head.back() == HeadTrack{0, -2, std::nullopt, -1});
  host.state_flags[0] = 0;
  CHECK(RunOne(host, Queued(15, {args}, 5)) == actors::kCommandIdle);
  CHECK(host.navigation.back() == Navigation{0, 0x0123, 6 * 4096, to_target});
}

TEST_CASE("Queued walks to a tile run for gait classes 2 and 0x40",
          "[actors][actor-command-queue]") {
  FakeCommandHost host = NavigationHost();
  const int to_target = FacingAlong(300, -300);
  const std::int32_t args = Halves(0x0123, 6);
  for (const std::uint8_t gait_class : std::to_array<std::uint8_t>({2, 0x40})) {
    host.gait.gait_class = gait_class;
    CHECK(RunOne(host, Queued(9, {args}, 5)) == actors::kCommandIdle);
    CHECK(host.navigation.back() == Navigation{0, 0x0123, 7 << 12, to_target});
  }
  host.gait.gait_class = 4;
  CHECK(RunOne(host, Queued(9, {args})) == actors::kCommandIdle);
  CHECK(host.navigation.back() == Navigation{0, 0x0123, 3 << 12, 0x0123});
}

TEST_CASE("Queued walks along a facing run for gait classes 4 and 0x80",
          "[actors][actor-command-queue]") {
  FakeCommandHost host = NavigationHost();
  const std::int32_t args = Halves(0x0123, 6);
  for (const std::uint8_t gait_class : std::to_array<std::uint8_t>({4, 0x80})) {
    host.gait.gait_class = gait_class;
    CHECK(RunOne(host, Queued(10, {args}, 5)) == actors::kCommandIdle);
    CHECK(host.navigation.back() == Navigation{0, 0x0123, 7 << 12, 0x0123});
  }
  host.gait.gait_class = 2;
  CHECK(RunOne(host, Queued(10, {args}, 5)) == actors::kCommandIdle);
  CHECK(host.navigation.back() == Navigation{0, 0x0123, 3 << 12, 0x0123});
  // Opcode 10 never faces its target.
  CHECK(std::ranges::count(host.placement_queries, 5) == 0);
}

TEST_CASE("Queued jump commands face their tile's centre",
          "[actors][actor-command-queue]") {
  const auto [opcode, jump] = GENERATE(
      std::pair<std::uint8_t, CommandJump>{11, CommandJump::kTile},
      std::pair<std::uint8_t, CommandJump>{12, CommandJump::kTile},
      std::pair<std::uint8_t, CommandJump>{14, CommandJump::kAirborne},
      std::pair<std::uint8_t, CommandJump>{18, CommandJump::kNavigation});
  FakeCommandHost host;
  host.placements[0].position = {.x = 100, .y = 0, .z = 900, .w = 0};
  const ActorCommand command = Queued(opcode, {Bytes(3, 1, 9, 0xAA), 0x5000});
  CHECK(RunOne(host, command) == actors::kCommandWaiting);
  REQUIRE(host.jumps.size() == 1);
  CHECK(host.jumps[0] ==
        Jump{0, jump, PackedTile{.x = 3, .layer = 1, .z = 9, .spare = 0xAA},
             0x5000, FacingAlong((3 * 128) + 64 - 100, (9 * 128) + 64 - 900)});
  CHECK(host.head.back() == HeadTrack{0, -2, std::nullopt, -1});
}

TEST_CASE("Queued teleports face the target's tile or keep the facing",
          "[actors][actor-command-queue]") {
  FakeCommandHost host;
  host.placements[0].position = {.x = 1, .y = 2, .z = 3, .w = 0x0456};
  host.placements[7].tile = {.x = 10, .layer = 0, .z = 2, .spare = 0};
  const std::int32_t tile = Bytes(4, 1, 6, 0);
  const PackedTile packed{.x = 4, .layer = 1, .z = 6, .spare = 0};

  CHECK(RunOne(host, Queued(23, {tile})) == actors::kCommandWaiting);
  CHECK(host.teleports.back() == Teleport{0, packed, 0x0456});
  CHECK(RunOne(host, Queued(23, {tile}, 7)) == actors::kCommandWaiting);
  CHECK(host.teleports.back() ==
        Teleport{0, packed, FacingAlong(6 * 4096, -4 * 4096)});
}

TEST_CASE("Queued flights normalize the target and pick their speed",
          "[actors][actor-command-queue]") {
  FakeCommandHost host = MoveHost();
  const auto normal =
      VS::Core::Math::NormalizeVector({300, -400, 1200}).direction;
  const MotionVector direction{.x = static_cast<std::int16_t>(normal.x),
                               .y = static_cast<std::int16_t>(normal.y),
                               .z = static_cast<std::int16_t>(normal.z)};
  const CommandArgs flight{Halves(300, -400), Halves(1200, 0)};

  host.gait.gait_class = 0x10;
  CHECK(RunOne(host, Queued(16, flight)) == actors::kCommandIdle);
  CHECK(host.flights.back() ==
        Move{0, direction, 7 << 12, FacingAlong(direction.x, direction.z)});
  CHECK(RunOne(host, Queued(17, flight, 5)) == actors::kCommandIdle);
  CHECK(host.flights.back() ==
        Move{0, direction, 3 << 12, FacingAlong(200, -100)});
  host.gait.gait_class = 0x20;
  CHECK(RunOne(host, Queued(17, flight)) == actors::kCommandIdle);
  CHECK(host.flights.back().speed == 7 << 12);
}

TEST_CASE("Queued vector moves difference the Q12 target and the position",
          "[actors][actor-command-queue]") {
  FakeCommandHost host = MoveHost();
  // Opcode 19: Q12 x, z, y targets less the position.
  const CommandArgs target{150 * 4096, 260 * 4096, -20 * 4096};
  const MotionVector step{.x = 50, .y = 30, .z = 60, .w = 0};
  CHECK(RunOne(host, Queued(19, target)) == actors::kCommandIdle);
  CHECK(host.vectors.back() == Move{0, step, 0, FacingAlong(50, 60)});
  CHECK(RunOne(host, Queued(19, target, 5)) == actors::kCommandIdle);
  CHECK(host.vectors.back() == Move{0, step, 0, FacingAlong(200, -100)});
}

TEST_CASE("Queued frame holds and combat poses",
          "[actors][actor-command-queue]") {
  FakeCommandHost host;
  host.placements[0].position = {.x = 1, .y = 2, .z = 3, .w = 0x0789};
  const std::int32_t frame = Halves(0xF00D, 0x0040);

  CHECK(RunOne(host, Queued(6, {frame})) == actors::kCommandIdle);
  CHECK(host.navigation.back() == Navigation{0, 0x0789, 0, 0x0789});
  CHECK(host.head.back() ==
        HeadTrack{0, -4, MotionVector{.y = static_cast<std::int16_t>(0xF00D)},
                  0x0040});
  CHECK(RunOne(host, Queued(7, {frame})) == actors::kCommandIdle);
  CHECK(host.head.back().step == 0x1000);

  CHECK(RunOne(host, Queued(20)) == actors::kCommandWaiting);
  CHECK(host.combat_poses.back() == std::pair<int, int>{0, 0x0789});
}

TEST_CASE("Queued ground stops and offsets", "[actors][actor-command-queue]") {
  FakeCommandHost host;
  CHECK(RunOne(host, Queued(8)) == actors::kCommandIdle);
  REQUIRE(host.ground.size() == 1);
  CHECK(host.ground[0].step == MotionVector{});
  host.state_flags[0] = actors::kStateMotionLocked;
  CHECK(RunOne(host, Queued(8)) == actors::kCommandIdle);
  CHECK(host.ground.size() == 1);

  host.state_flags[0] = 0;
  CHECK(RunOne(host, Queued(13, {0x3000, -0x2000, 0})) == actors::kCommandIdle);
  CHECK(host.ground.back().step == MotionVector{.x = 3, .y = 0, .z = -2});
}

TEST_CASE("Queued travel commands face their target or destination",
          "[actors][actor-command-queue]") {
  FakeCommandHost host;
  host.placements[0].tile = {.x = 4, .layer = 0, .z = 9, .spare = 0};
  host.placements[0].position = {.x = 10, .y = 0, .z = -20, .w = 0};
  host.placements[5].position = {.x = 50, .y = 0, .z = 80, .w = 0};
  const CommandArgs args{Halves(-0x7000, 0x1000), Halves(0x6000, 0x0102)};
  const MotionVector destination{
      .x = -0x7000, .y = 0x1000, .z = 0x6000, .w = 0x0102};

  CHECK(RunOne(host, Queued(22, args)) == actors::kCommandWaiting);
  CHECK(host.travels.back() == Travel{0, TravelPose::kTimed, destination,
                                      FacingAlong(-7 - 10, 6 + 20)});
  CHECK(RunOne(host, Queued(22, args, 5)) == actors::kCommandWaiting);
  CHECK(host.travels.back().facing == FacingAlong(40, 100));

  // The tile bytes are under one Q12 unit: the step is the tile's negation.
  CHECK(RunOne(host, Queued(21, args)) == actors::kCommandWaiting);
  CHECK(host.travels.back() ==
        Travel{0, TravelPose::kTile, destination, FacingAlong(-4, -9)});
  CHECK(RunOne(host, Queued(21, args, 5)) == actors::kCommandWaiting);
  CHECK(host.travels.back().facing == FacingAlong(40, 100));
}

TEST_CASE("Queued limb actions select the action or the default",
          "[actors][actor-command-queue]") {
  FakeCommandHost host;
  host.limb_action = 0x0042;
  CHECK(RunOne(host, Queued(24, {Halves(3, 2)})) == actors::kCommandIdle);
  CHECK(host.limb_queries.back() == std::pair<unsigned, unsigned>{3, 2});
  CHECK(host.selections.back() == std::pair<int, std::uint16_t>{0, 0x0042});
  CHECK(host.head.back() == HeadTrack{0, -2, std::nullopt, -1});

  host.limb_action = 0;
  host.preview_missing = true;
  CHECK(RunOne(host, Queued(24, {Halves(-1, 9)})) ==
        actors::kCommandActionPreviewMissing);
  CHECK(host.limb_queries.back() == std::pair<unsigned, unsigned>{0xFFFF, 9});
  CHECK(host.selections.back().second == 1);

  host.limb_action = std::nullopt;
  host.preview_missing = false;
  CHECK(RunOne(host, Queued(24, {Halves(7, 0)})) == actors::kCommandIdle);
  CHECK(host.selections.back().second == 1);
}

namespace {
// The command fields of a battle actor, compared as one value.
struct LiveCommandFields {
  std::uint8_t state = 0;
  std::uint8_t opcode = 0;
  std::uint8_t target = 0;
  std::uint8_t flags = 0;
  CommandArgs args{};
  bool operator==(const LiveCommandFields&) const = default;
};

LiveCommandFields FieldsOf(const Battle_Actor& actor) {
  return {
      .state = actor.commandState,
      .opcode = actor.commandOpcode,
      .target = actor.commandTarget,
      .flags = actor.commandFlags,
      .args = {actor.commandArg0, actor.commandArg1.word, actor.commandArg2}};
}

std::array<int, 3> StepOf(const Actors_ActorRecord& record) {
  return {record.turnState.step.vx, record.turnState.step.vy,
          record.turnState.step.vz};
}

// Swaps in empty actor tables and restores the live ones on scope exit.
class LiveActorTables {
 public:
  LiveActorTables()
      : records_(std::to_array(Actors_actorRecords)),
        battle_actors_(std::to_array(Battle_actors)),
        control_mode_(Field_controlMode) {
    std::ranges::fill(Actors_actorRecords, nullptr);
    std::ranges::fill(Battle_actors, nullptr);
    Field_controlMode = 0;
  }
  ~LiveActorTables() {
    std::ranges::copy(records_, std::begin(Actors_actorRecords));
    std::ranges::copy(battle_actors_, std::begin(Battle_actors));
    Field_controlMode = control_mode_;
  }
  LiveActorTables(const LiveActorTables&) = delete;
  LiveActorTables& operator=(const LiveActorTables&) = delete;

 private:
  decltype(std::to_array(Actors_actorRecords)) records_;
  decltype(std::to_array(Battle_actors)) battle_actors_;
  u_int control_mode_;
};
}  // namespace

TEST_CASE("The live executor steps a waiting ground offset in the actor tables",
          "[actors][actor-command-queue]") {
  const LiveActorTables tables;
  const auto record = std::make_unique<Actors_ActorRecord>();
  record->instance.attachParent = 0xFF;
  Actors_actorRecords[0] = record.get();
  const auto battle_actor = std::make_unique<Battle_Actor>();
  battle_actor->kind = 2;
  battle_actor->commandState = actors::kCommandWaiting;
  battle_actor->commandOpcode = 13;
  battle_actor->commandTarget = kNoTarget;
  battle_actor->commandFlags = 0x5A;
  battle_actor->commandArg0 = 0x9000;
  battle_actor->commandArg1.word = -0x3000;
  battle_actor->commandArg2 = 0x2000;
  Battle_actors[0] = battle_actor.get();

  CHECK(actors::ExecuteLiveQueuedActorCommands() == 0);
  CHECK(FieldsOf(*battle_actor) ==
        LiveCommandFields{.state = actors::kCommandWaiting,
                          .opcode = 13,
                          .target = kNoTarget,
                          .flags = 0x5A,
                          .args = {0x1000, 0, 0}});
  CHECK(StepOf(*record) == std::array<int, 3>{8, 2, -3});

  CHECK(actors::ExecuteLiveQueuedActorCommands() == 0);
  CHECK(FieldsOf(*battle_actor) ==
        LiveCommandFields{.state = actors::kCommandIdle,
                          .opcode = 13,
                          .target = kNoTarget,
                          .flags = 0x5A,
                          .args = {0, 0, 0}});
  CHECK(StepOf(*record)[0] == 1);
}

namespace {
struct DirectMove {
  bool jump = false;
  int direction = 0;
  int speed = 0;
  bool operator==(const DirectMove&) const = default;
};

class FakeAshleyHost final : public actors::AshleyMovementHost {
 public:
  actors::CommandActorGait gait{
      .walk_speed = 5, .gait_class = 0, .run_speed = 9};
  std::uint32_t state_flags = 0;
  unsigned transition_step = 0;
  std::optional<int> door_elevation;
  actors::AshleyControls controls{};
  int camera_distance = 2000;
  int camera_yaw = 0x0900;

  std::vector<std::string> calls;
  std::optional<AshleyMoveInput> move_input;
  std::vector<DirectMove> moves;

  void ConstrainCameraToPlayer() override { calls.emplace_back("constrain"); }
  [[nodiscard]] actors::CommandActorGait PlayerGait() const override {
    return gait;
  }
  [[nodiscard]] std::uint32_t PlayerStateFlags() const override {
    return state_flags;
  }
  [[nodiscard]] unsigned ScreenTransitionStep() const override {
    return transition_step;
  }
  [[nodiscard]] std::optional<int> DoorEntryElevation() const override {
    return door_elevation;
  }
  void UpdateCameraOrbit() override { calls.emplace_back("orbit"); }
  [[nodiscard]] actors::AshleyControls Controls() const override {
    return controls;
  }
  [[nodiscard]] int CameraDistance() const override { return camera_distance; }
  void SetCameraDistance(int distance) override { camera_distance = distance; }
  [[nodiscard]] int CameraYaw() const override { return camera_yaw; }
  void SetMoveInput(AshleyMoveInput input) override { move_input = input; }
  void SetDirectMovement(int direction, int speed) override {
    moves.push_back({false, direction, speed});
  }
  void BeginJumpMovement(int direction, int distance) override {
    moves.push_back({true, direction, distance});
  }
};
}  // namespace

TEST_CASE("Ashley's stick input drops its dead zone",
          "[actors][ashley-input]") {
  CHECK_FALSE(actors::AshleyStickMoveInput({128, 128}).has_value());
  CHECK_FALSE(actors::AshleyStickMoveInput({192, 64}).has_value());
  CHECK(actors::AshleyStickMoveInput({193, 128}) ==
        AshleyMoveInput{.lateral = -1, .forward = 0});
  CHECK(actors::AshleyStickMoveInput({200, 128}) ==
        AshleyMoveInput{.lateral = -8, .forward = 0});
  CHECK(actors::AshleyStickMoveInput({0, 255}) ==
        AshleyMoveInput{.lateral = 64, .forward = 63});
  // One axis outside the dead zone counts; the other keeps its dead zone.
  CHECK(actors::AshleyStickMoveInput({150, 10}) ==
        AshleyMoveInput{.lateral = 0, .forward = -54});
}

TEST_CASE("Ashley's direction pad, camera distance and move direction",
          "[actors][ashley-input]") {
  using Pad = actors::AshleyDirectionPad;
  CHECK(actors::AshleyDirectionPadMoveInput(Pad{.down = true}) ==
        AshleyMoveInput{.lateral = 0, .forward = 16});
  CHECK(actors::AshleyDirectionPadMoveInput(Pad{.up = true, .down = true}) ==
        AshleyMoveInput{});
  CHECK(actors::AshleyDirectionPadMoveInput(Pad{.up = true, .left = true}) ==
        AshleyMoveInput{.lateral = 16, .forward = -16});
  CHECK(actors::AshleyDirectionPadMoveInput(Pad{.right = true}) ==
        AshleyMoveInput{.lateral = -16, .forward = 0});

  CHECK(actors::ClampAshleyCameraDistance(1000) == 1536);
  CHECK(actors::ClampAshleyCameraDistance(2000) == 2000);
  CHECK(actors::ClampAshleyCameraDistance(3000) == 2304);

  const AshleyMoveInput input{.lateral = -8, .forward = 3};
  CHECK(actors::AshleyMoveDirection(input, 0x0900) ==
        ((VS::Core::Math::Arctangent2(-8, 3) + 0x0900 + 0x800) & 0xFFF));
}

TEST_CASE("Ashley walks in from a door during the screen transition",
          "[actors][ashley-input]") {
  for (const unsigned step : {1U, 2U, 4U}) {
    FakeAshleyHost host;
    host.transition_step = step;
    host.door_elevation = 3;
    host.controls.jump_pressed = true;
    actors::UpdateAshleyMovementInput(host);
    CHECK(host.calls == std::vector<std::string>{"constrain"});
    CHECK(host.moves == std::vector<DirectMove>{{false, 3 << 10, 5 * 2048}});
    CHECK_FALSE(host.move_input.has_value());

    host.moves.clear();
    host.door_elevation = std::nullopt;
    actors::UpdateAshleyMovementInput(host);
    CHECK(host.moves == std::vector<DirectMove>{{false, 0, 0}});
  }

  FakeAshleyHost host;
  host.transition_step = 3;
  host.door_elevation = 3;
  host.camera_distance = 4000;
  actors::UpdateAshleyMovementInput(host);
  CHECK(host.calls == std::vector<std::string>{"constrain", "orbit"});
  CHECK(host.camera_distance == 2304);
  CHECK(host.move_input == AshleyMoveInput{});
}

namespace {
// Runs Ashley's movement input once and returns the one move it requested.
DirectMove RunAshley(FakeAshleyHost& host) {
  host.moves.clear();
  actors::UpdateAshleyMovementInput(host);
  REQUIRE(host.moves.size() == 1);
  return host.moves[0];
}

// Ashley's stick is at rest and the direction pad is held left.
FakeAshleyHost AshleyPadHost() {
  FakeAshleyHost host;
  host.controls.stick = actors::AshleyStick{.x = 128, .y = 140};
  host.controls.held.left = true;
  return host;
}
}  // namespace

TEST_CASE("Ashley runs, walks or jumps by the direction pad and her state",
          "[actors][ashley-input]") {
  FakeAshleyHost host = AshleyPadHost();
  const AshleyMoveInput pad{.lateral = 16, .forward = 0};
  const int direction = actors::AshleyMoveDirection(pad, host.camera_yaw);

  // A stick at rest falls back to the direction pad.
  CHECK(RunAshley(host) == DirectMove{false, direction, 9 * 4096});
  CHECK(host.move_input == pad);
  host.controls.jump_pressed = true;
  CHECK(RunAshley(host) == DirectMove{true, direction, 9 * 4096});
  host.controls.walk_held = true;
  CHECK(RunAshley(host) == DirectMove{true, direction, 5 * 4096});
  host.controls.jump_pressed = false;
  CHECK(RunAshley(host) == DirectMove{false, direction, 5 * 4096});
  host.state_flags = actors::kStateActionPose;
  host.controls.walk_held = false;
  host.controls.jump_pressed = true;
  CHECK(RunAshley(host) == DirectMove{false, direction, 5 * 4096});
}

TEST_CASE("Ashley's stick wins over the direction pad outside its dead zone",
          "[actors][ashley-input]") {
  FakeAshleyHost host = AshleyPadHost();
  host.controls.stick = actors::AshleyStick{.x = 255, .y = 0};
  const AshleyMoveInput stick{.lateral = -63, .forward = -64};
  CHECK(
      RunAshley(host) ==
      DirectMove{false, actors::AshleyMoveDirection(stick, 0x0900), 9 * 4096});
  CHECK(host.move_input == stick);
}

TEST_CASE("Without input Square jumps in place unless Ashley is walk-only",
          "[actors][ashley-input]") {
  FakeAshleyHost host;
  host.controls.jump_pressed = true;
  CHECK(RunAshley(host) ==
        DirectMove{true, actors::AshleyMoveDirection({}, 0x0900), 0});
  host.state_flags = actors::kStateActionPose;
  const int rest = VS::Core::Math::Arctangent2(0, 0) & 0xFFF;
  CHECK(RunAshley(host) == DirectMove{false, rest, 0});
  host.controls.jump_pressed = false;
  host.state_flags = 0;
  CHECK(RunAshley(host) == DirectMove{false, rest, 0});
}

TEST_CASE("Promoted actor commands leave no module function binding",
          "[actors][actor-command-queue][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  // No module image stores these functions' addresses; callers use the typed
  // API by name.
  constexpr std::array<std::uint32_t, 3> kRetired{0x8006E7F0U, 0x800753F8U,
                                                  0x80075554U};
  for (const std::uint32_t address : kRetired) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address) == nullptr);
  }
  // The move input buffer stays a module data symbol at its own base.
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800F1A50U) ==
        static_cast<void*>(Actors_ashleyMoveInput));
}
