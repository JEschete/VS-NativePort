#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iterator>
#include <numeric>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/ActionPhases.hpp"
#include "Battle/ActionPhasesLive.hpp"
#include "Content/ModuleData.h"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
}

namespace {
namespace b = VS::Battle;
namespace a = VS::Actors;
enum class Operation {
  kCommand,
  kWriteCommand,
  kFlags,
  kIdle,
  kPalette,
  kFreeze,
  kClut,
  kRoom,
  kMarkers,
  kReward,
  kSpeed,
  kPhase,
  kControl,
  kBone,
  kFade,
  kToggle
};
struct Event {
  Operation operation{};
  int first{}, second{};
  bool operator==(const Event&) const = default;
};
struct Source final : b::ActionPhaseSource {
  std::uint8_t actor{};
  std::uint8_t Actor() const override { return actor; }
};
struct World final : b::ActionPhaseWorld {
  std::array<std::optional<a::ActorCommand>, 17> commands{};
  std::array<std::array<std::uint32_t, 2>, 16> flags{};
  std::array<std::size_t, 16> flag_reads{};
  std::vector<Event> events;
  std::function<void(Event)> callback;
  std::uint8_t primary{};
  std::int32_t reward{};
  std::uint32_t speed{};
  b::ActionPhase phase = b::ActionPhase::kInitial;
  VS::Field::FieldControlMode control = VS::Field::FieldControlMode::kNone;
  int bone = -1;

  void Record(Event event) {
    events.push_back(event);
    if (callback) callback(event);
  }
  std::optional<a::ActorCommand> Command(int slot) override {
    Record({Operation::kCommand, slot});
    return commands.at(static_cast<std::size_t>(slot));
  }
  void SetCommandState(int slot, std::uint8_t state) override {
    auto& command = commands.at(static_cast<std::size_t>(slot));
    if (command.has_value()) {
      command.value().state = state;
    } else {
      FAIL("Writing a command requires a present actor");
    }
    Record({Operation::kWriteCommand, slot, state});
  }
  std::uint8_t PrimaryActor() override { return primary; }
  std::uint32_t ActorFlags(int slot) override {
    const auto index = static_cast<std::size_t>(slot);
    Record({Operation::kFlags, slot});
    return flags.at(index).at(flag_reads.at(index)++);
  }
  void RestoreIdle(int slot) override { Record({Operation::kIdle, slot}); }
  void RestoreRoomPalettes() override { Record({Operation::kPalette}); }
  void FreezeAnimations() override { Record({Operation::kFreeze}); }
  void EnableClutAnimation() override { Record({Operation::kClut}); }
  void ResumeRoomAnimations() override { Record({Operation::kRoom}); }
  void ShowActorMarkers() override { Record({Operation::kMarkers}); }
  void SetRecoveryReward(std::int32_t mode) override {
    reward = mode;
    Record({Operation::kReward, mode});
  }
  void SetTickSpeed(std::uint32_t value) override {
    speed = value;
    Record({Operation::kSpeed, static_cast<int>(value)});
  }
  void SetPhase(b::ActionPhase value) override {
    phase = value;
    Record({Operation::kPhase, static_cast<int>(value)});
  }
  void SetControlMode(VS::Field::FieldControlMode value) override {
    control = value;
    Record({Operation::kControl, static_cast<int>(value)});
  }
  int TrailBone(std::uint8_t actor) override {
    Record({Operation::kBone, actor});
    return bone;
  }
  void FadeTrails(std::uint8_t actor, int value) override {
    Record({Operation::kFade, actor, value});
  }
  void ToggleAshleyWeapon() override { Record({Operation::kToggle}); }
};

struct LiveStateReset {
  std::uint32_t phase = Battle_actionFlowMode;
  Battle_ActionState* state = Battle_actionState;
  ~LiveStateReset() {
    Battle_actionFlowMode = phase;
    Battle_actionState = state;
  }
};
struct LiveActorsReset {
  std::array<Battle_Actor*, 42> actors{};
  LiveActorsReset() {
    std::copy(std::begin(Battle_actors), std::end(Battle_actors),
              actors.begin());
  }
  ~LiveActorsReset() {
    std::copy(actors.begin(), actors.end(), std::begin(Battle_actors));
  }
};

TEST_CASE(
    "F00001 releases only queued attack commands in the first sixteen slots",
    "[action-phases][F00001]") {
  World world;
  world.commands.at(0).emplace(a::ActorCommand{.state = 0x80, .opcode = 0x18});
  world.commands.at(1).emplace(a::ActorCommand{.state = 0x80, .opcode = 0x17});
  world.commands.at(2).emplace(a::ActorCommand{.state = 0x40, .opcode = 0x18});
  world.commands.at(15) = world.commands.at(0);
  world.commands.at(16) = world.commands.at(0);
  b::ReleaseQueuedActorAttacks(world);
  CHECK(world.commands.at(0) == a::ActorCommand{.state = 8, .opcode = 0x18});
  CHECK(world.commands.at(15) == a::ActorCommand{.state = 8, .opcode = 0x18});
  CHECK(world.commands.at(1) == a::ActorCommand{.state = 0x80, .opcode = 0x17});
  CHECK(world.commands.at(2) == a::ActorCommand{.state = 0x40, .opcode = 0x18});
  CHECK(world.commands.at(16) ==
        a::ActorCommand{.state = 0x80, .opcode = 0x18});
  CHECK(world.events.back() == Event{Operation::kWriteCommand, 15, 8});
}

TEST_CASE("F00089 commits movement modes before restoring Ashley",
          "[action-phases][F00089]") {
  World world;
  world.callback = [&](Event event) {
    if (event.operation == Operation::kIdle) {
      CHECK(world.control == VS::Field::FieldControlMode::kGimmickMotion);
      CHECK(world.phase == b::ActionPhase::kInitial);
      world.phase = b::ActionPhase::kAbilityFinished;
    }
  };
  b::WaitForPostActionMovement(world);
  CHECK(world.events == std::vector<Event>{{Operation::kControl, 12},
                                           {Operation::kPhase, 0},
                                           {Operation::kIdle, 0}});
  CHECK(world.phase == b::ActionPhase::kAbilityFinished);
}

TEST_CASE(
    "F00001 live release preserves native slots beyond the original roster",
    "[action-phases][F00001]") {
  const LiveActorsReset reset;
  std::fill(std::begin(Battle_actors), std::end(Battle_actors), nullptr);
  Battle_Actor first{}, last{}, extra{};
  first.commandState = last.commandState = extra.commandState = 0x80;
  first.commandOpcode = last.commandOpcode = extra.commandOpcode = 0x18;
  Battle_actors[0] = &first;
  Battle_actors[15] = &last;
  Battle_actors[16] = &extra;
  b::ReleaseLiveQueuedActorAttacks();
  CHECK(first.commandState == 8);
  CHECK(last.commandState == 8);
  CHECK(extra.commandState == 0x80);
  CHECK(first.commandOpcode == 0x18);
}

TEST_CASE("F00096 rereads source ownership and both actor flag results",
          "[action-phases][F00096]") {
  World world;
  world.primary = 3;
  world.flags.at(0) = {0, 1};
  world.flags.at(1) = {a::kStateActionAnimating, 1};
  world.flags.at(3) = {1, 1};
  world.callback = [&](Event event) {
    if (event == Event{Operation::kIdle, 0}) world.primary = 2;
  };
  b::BeginPostActionRecovery(-17, world);
  CHECK(world.flag_reads.at(0) == 2);
  CHECK(world.flag_reads.at(1) == 1);
  CHECK(world.flag_reads.at(2) == 0);
  CHECK(world.flag_reads.at(3) == 2);
  CHECK(world.events.at(2) == Event{Operation::kIdle, 0});
  CHECK(world.reward == -17);
}

TEST_CASE(
    "F00096 restores presentation before committing current recovery state",
    "[action-phases][F00096]") {
  World world;
  world.callback = [&](Event event) {
    if (event.operation == Operation::kMarkers) {
      world.reward = 99;
      world.speed = 77;
      world.phase = b::ActionPhase::kAbilityFinished;
    }
  };
  b::BeginPostActionRecovery(-1, world);
  const std::vector<Event> tail(world.events.end() - 8, world.events.end());
  CHECK(tail == std::vector<Event>{{Operation::kPalette},
                                   {Operation::kFreeze},
                                   {Operation::kClut},
                                   {Operation::kRoom},
                                   {Operation::kMarkers},
                                   {Operation::kReward, -1},
                                   {Operation::kSpeed, 2},
                                   {Operation::kPhase, 11}});
  CHECK(world.reward == -1);
  CHECK(world.speed == 2);
  CHECK(world.phase == b::ActionPhase::kRecovery);
}

TEST_CASE(
    "F00097 retains the initial source but rereads its actor after the query",
    "[action-phases][F00097]") {
  World world;
  Source first, replacement;
  first.actor = 3;
  replacement.actor = 9;
  Source* current = &first;
  world.bone = -99;
  world.callback = [&](Event event) {
    if (event.operation == Operation::kBone) {
      CHECK(world.phase == b::ActionPhase::kTrailFade);
      first.actor = 7;
      current = &replacement;
    }
  };
  b::BeginActionTrailFade(*current, world);
  CHECK(current == &replacement);
  CHECK(world.events == std::vector<Event>{{Operation::kPhase, 10},
                                           {Operation::kBone, 3},
                                           {Operation::kFade, 7, -99}});
}

TEST_CASE("F00098 live completion does not require an action-state record",
          "[action-phases][F00098]") {
  const LiveStateReset reset;
  Battle_actionState = nullptr;
  Battle_actionFlowMode = 0xFFFFFFFF;
  b::FinishLiveBattleAbilitySequence();
  CHECK(Battle_actionFlowMode == 9);
  CHECK(Battle_actionState == nullptr);
}

TEST_CASE("F00114 live reset leaves the unused action-state owner untouched",
          "[action-phases][F00114]") {
  const LiveStateReset reset;
  Battle_actionState = nullptr;
  Battle_actionFlowMode = 0xFFFFFFFF;
  b::ResetLiveActionPhase();
  CHECK(Battle_actionFlowMode == 0);
  CHECK(Battle_actionState == nullptr);
}

TEST_CASE("F00121 settles every actor before toggling and selecting field mode",
          "[action-phases][F00121]") {
  World world;
  world.callback = [&](Event event) {
    if (event.operation == Operation::kToggle) {
      world.control = VS::Field::FieldControlMode::kFirstPersonView;
    }
  };
  b::BeginAshleyWeaponToggle(world);
  std::vector<int> slots;
  for (const Event event : world.events) {
    if (event.operation == Operation::kIdle) slots.push_back(event.first);
  }
  std::vector<int> expected(16);
  std::iota(expected.begin(), expected.end(), 0);
  CHECK(slots == expected);
  CHECK(world.events.at(16) == Event{Operation::kToggle});
  CHECK(world.events.at(17) == Event{Operation::kControl, 10});
  CHECK(world.control == VS::Field::FieldControlMode::kWeaponToggle);
}

TEST_CASE("F00122 forwarding retirement retains native phase entrypoints",
          "[action-phases][F00122]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8007350C) == nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800734C0) ==
        reinterpret_cast<void*>(&b::BeginLiveAshleyWeaponToggle));
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8006FCBC) ==
        reinterpret_cast<void*>(&b::BeginLiveActionTrailFade));
}
}  // namespace
