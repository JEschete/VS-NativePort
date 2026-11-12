#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/ActionExecution.hpp"
#include "Battle/Ai/ActionExecutionLive.hpp"
#include "Content/ModuleData.h"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
}

namespace {
namespace ai = VS::Battle::Ai;
template <typename T>
T& Value(std::optional<T>& value) {
  if (!value) throw std::logic_error("Missing action execution fixture value");
  return value.value();
}

class Execution final : public ai::ActionExecutionWorld {
 public:
  int count = 1;
  std::array<std::optional<ai::ActionCandidate>, 3> entries{
      ai::ActionCandidate{}, ai::ActionCandidate{}, ai::ActionCandidate{}};
  std::array<std::optional<std::size_t>, 2> ranks{0, 1};
  std::array<std::optional<int>, 4> special{};
  std::array<std::optional<bool>, 16> gimmicks{};
  std::vector<std::string> calls;
  bool can = true, ready = true, moving = true, room = true, cooldown = true;
  bool room_delay = true;
  int random = 10;
  int selected = -1;
  bool engaged = false;
  unsigned delay = 7;
  std::uint8_t room_default = 201;
  std::function<void(const std::string&)> callback;
  int CandidateCount() const override { return count; }
  std::optional<std::size_t> RankedCandidate(std::size_t rank) const override {
    return rank < ranks.size() ? ranks.at(rank) : std::nullopt;
  }
  std::size_t FallbackCandidate() const override { return 2; }
  std::optional<ai::ActionCandidate> Candidate(
      std::size_t handle) const override {
    return handle < entries.size() ? entries.at(handle) : std::nullopt;
  }
  void StoreCandidate(std::size_t handle,
                      const ai::ActionCandidate& candidate) override {
    entries.at(handle) = candidate;
  }
  void Note(const std::string& call) {
    calls.push_back(call);
    if (callback) callback(call);
  }
  bool CanExecute(std::size_t handle) override {
    Note("can" + std::to_string(handle));
    return can;
  }
  void TryEngage() override { Note("engage"); }
  bool EngagementReady() override {
    Note("ready");
    return ready;
  }
  void SelectCandidate(std::size_t handle) override {
    selected = static_cast<int>(handle);
    engaged = true;
  }
  bool TargetMoving(unsigned) const override { return moving; }
  int RandomMasked(int mask) override {
    Note("random" + std::to_string(mask));
    return random;
  }
  void IssueCommand(int command) override {
    Note("command" + std::to_string(command));
  }
  bool RequiresCooldown(std::size_t handle) override {
    Note("cooldown" + std::to_string(handle));
    return cooldown;
  }
  bool UsesRoomDelay() const override { return room_delay; }
  std::optional<std::uint8_t> RoomDelay() const override {
    return room ? std::optional(room_default) : std::nullopt;
  }
  void SetDelay(std::uint8_t value) override { delay = value; }
  bool HasRoom() const override { return room; }
  std::optional<int> SpecialActor(unsigned slot) const override {
    return special.at(slot);
  }
  std::optional<bool> GimmickReady(int actor) const override {
    return actor >= 0 && actor < static_cast<int>(gimmicks.size())
               ? gimmicks.at(actor)
               : std::nullopt;
  }
};

class Decisions final : public ai::DecisionUpdateWorld {
 public:
  std::array<int, 4> ids{0, 1, 2, 3};
  std::array<std::size_t, 4> next{2, 3, 0, 0};
  std::array<bool, 4> present{true, true, true, true};
  std::array<ai::DecisionDispatchState, 4> states{};
  std::array<unsigned, 4> distances{};
  std::size_t head = 1;
  int selected = 0;
  int elapsed = 10, budget = 10, cached = -1;
  std::optional<ai::DecisionPlayer> player = ai::DecisionPlayer{};
  std::optional<VS::Core::Math::Position3> battle_position =
      VS::Core::Math::Position3{100, 20, 30};
  std::optional<std::uint16_t> flags = 0xFEAB;
  std::array<unsigned, 3> directions{12, 34, 56};
  std::vector<std::string> calls;
  std::function<void(const std::string&)> callback;
  std::size_t ActorHead() const override { return head; }
  std::optional<int> ActorId(std::size_t node) const override {
    return node > 0 && node <= ids.size() ? std::optional(ids.at(node - 1))
                                          : std::nullopt;
  }
  std::size_t ActorNext(std::size_t node) const override {
    return next.at(node - 1);
  }
  bool SelectActor(int id) override {
    selected = id;
    return present.at(id);
  }
  ai::DecisionDispatchState State() const override {
    return states.at(selected);
  }
  void Note(const std::string& call) {
    calls.push_back(call + ":" + std::to_string(selected));
    if (callback) callback(call);
  }
  int ElapsedTicks() override {
    Note("elapsed");
    return elapsed;
  }
  int TickBudget() const override { return budget; }
  void CacheTick(int tick) override { cached = tick; }
  void IssueCommand(int command) override {
    Note("command" + std::to_string(command));
  }
  std::optional<ai::DecisionPlayer> Player() const override { return player; }
  std::optional<VS::Core::Math::Position3> PlayerBattlePosition()
      const override {
    return battle_position;
  }
  void StorePlayerDistance(std::uint32_t distance) override {
    distances.at(selected) = distance;
  }
  std::optional<int> ExecutePlannedActions() override {
    Note("execute");
    return 0;
  }
  std::optional<std::uint16_t> TileFlags(unsigned x,
                                         unsigned z) const override {
    return x < 32 && z < 32 ? flags : std::nullopt;
  }
  void ResetMovement(std::uint8_t area) override {
    Note("reset" + std::to_string(area));
  }
  void ClearFirstDirectionWord() override { directions.at(0) = 0; }
  void DispatchMovement() override { Note("dispatch"); }
  void UpdateLocomotion() override { Note("locomotion"); }
};

struct LiveDecisionFixture {
  Battle_Ai_FrameContext old_frame = *Battle_Ai_FrameState();
  Battle_Actor* old_head = Battle_actors[0];
  Battle_Ai_ActorAiState* old_player = Battle_Ai_actorAiStates[0];
  Actors_ActorRecord* old_record = Actors_actorRecords[0];
  int old_budget = Battle_Ai_frameTickBudget;
  Battle_Actor head{}, actor{};
  Battle_Ai_ActorAiState player{}, state{};
  Actors_ActorRecord record{};
  Actors_ModelInstance model{};
  LiveDecisionFixture() {
    head.next = &actor;
    head.model = &model;
    actor.id = 1;
    state.reserved14[0] = 1;
    state.reserved1D[0] = 1;
    Battle_actors[0] = &head;
    Battle_Ai_FrameState()->actors[1] = &state;
    Battle_Ai_actorAiStates[0] = &player;
    Actors_actorRecords[0] = &record;
    Battle_Ai_frameTickBudget = std::numeric_limits<int>::max();
  }
  ~LiveDecisionFixture() {
    *Battle_Ai_FrameState() = old_frame;
    Battle_actors[0] = old_head;
    Battle_Ai_actorAiStates[0] = old_player;
    Actors_actorRecords[0] = old_record;
    Battle_Ai_frameTickBudget = old_budget;
  }
};
}  // namespace

TEST_CASE("Action execution gates run in original order",
          "[action-execution][F01657]") {
  Execution world;
  world.can = false;
  CHECK(ai::TryExecuteActionCandidate(world, 0) == 0);
  CHECK(world.calls == std::vector<std::string>{"can0"});
  world.calls.clear();
  world.can = true;
  world.ready = false;
  CHECK(ai::TryExecuteActionCandidate(world, 0) == 0);
  CHECK(world.calls == std::vector<std::string>{"can0", "engage", "ready"});
  CHECK(world.selected == -1);
  CHECK_FALSE(world.engaged);
}

TEST_CASE(
    "Moving action publishes pointer before random and preserves callback "
    "words",
    "[action-execution][F01657]") {
  Execution world;
  bool published = false;
  world.callback = [&](const std::string& call) {
    if (call == "random2") {
      published = world.selected == 0 && world.engaged;
      Value(world.entries.at(0)).SetTargetId(9);
      Value(world.entries.at(0)).SetWeight(17);
    }
  };
  CHECK(ai::TryExecuteActionCandidate(world, 0) == 1);
  CHECK(published);
  CHECK(Value(world.entries.at(0)).TargetId() == 9);
  CHECK(Value(world.entries.at(0)).TargetLimb() == 2);
  CHECK(Value(world.entries.at(0)).HeightOffset() == 217);
  CHECK(Value(world.entries.at(0)).Weight() == 17);
  CHECK(world.calls == std::vector<std::string>{"can0", "engage", "ready",
                                                "random2", "command15",
                                                "cooldown0"});
  CHECK(world.delay == 201);
}

TEST_CASE("Action cooldown uses post-callback actor mode",
          "[action-execution][F01657]") {
  Execution world;
  world.moving = false;
  world.room = false;
  world.callback = [&](const std::string& call) {
    if (call == "cooldown0") world.room_delay = false;
  };
  CHECK(ai::TryExecuteActionCandidate(world, 0) == 1);
  CHECK(world.delay == 50);
  CHECK(world.calls == std::vector<std::string>{"can0", "engage", "ready",
                                                "command15", "cooldown0"});
  world.cooldown = false;
  world.delay = 19;
  CHECK(ai::TryExecuteActionCandidate(world, 0) == 1);
  CHECK(world.delay == 19);
}

TEST_CASE("Execution missing inputs refuse rather than default",
          "[action-execution][F01657]") {
  Execution world;
  CHECK_FALSE(ai::TryExecuteActionCandidate(world, 9));
  world.room = false;
  CHECK_FALSE(ai::TryExecuteActionCandidate(world, 0));
  CHECK(world.delay == 7);
  CHECK_FALSE(ai::TryExecuteCheckedLiveActionCandidate(nullptr, nullptr));
  Battle_Ai_ActorAiState state{};
  CHECK_FALSE(ai::TryExecuteCheckedLiveActionCandidate(&state, nullptr));
}

TEST_CASE("Ranked execution rereads count after a rejected candidate",
          "[action-execution][F01658]") {
  Execution world;
  world.count = 2;
  world.can = false;
  world.callback = [&](const std::string& call) {
    if (call == "can0") world.count = 0;
  };
  CHECK(ai::TryExecutePlannedActions(world) == 0);
  CHECK(world.calls == std::vector<std::string>{"can0"});
  world.calls.clear();
  world.count = 1;
  world.callback = [&](const std::string& call) {
    if (call == "can0") world.count = 2;
    if (call == "can1") world.can = true;
  };
  CHECK(ai::TryExecutePlannedActions(world) == 1);
  CHECK(world.selected == 1);
}

TEST_CASE("Fallback visits four room slots and leaves packed neighbors",
          "[action-execution][F01658]") {
  Execution world;
  world.count = 0;
  Value(world.entries.at(2)).SetAction(5);
  Value(world.entries.at(2)).SetHeightOffset(777);
  world.moving = false;
  world.special = {1, 2, std::nullopt, 3};
  world.gimmicks.at(1) = false;
  world.gimmicks.at(2) = false;
  world.gimmicks.at(3) = true;
  CHECK(ai::TryExecutePlannedActions(world) == 1);
  CHECK(world.selected == 2);
  CHECK(Value(world.entries.at(2)).TargetId() == 3);
  CHECK(Value(world.entries.at(2)).HeightOffset() == 777);
  CHECK(world.calls.front() == "can2");
}

TEST_CASE("Fallback truncates target but validates full actor ID",
          "[action-execution][F01658]") {
  Execution world;
  world.count = 0;
  Value(world.entries.at(2)).SetAction(5);
  world.special.at(0) = 17;
  world.gimmicks.at(1) = true;
  CHECK_FALSE(ai::TryExecutePlannedActions(world));
  CHECK(Value(world.entries.at(2)).TargetId() == 1);
  CHECK(world.calls.empty());
  world.special.at(0) = -1;
  CHECK_FALSE(ai::TryExecutePlannedActions(world));
  CHECK(Value(world.entries.at(2)).TargetId() == 15);
}

TEST_CASE("Planned execution validates counts ranks room and gimmicks",
          "[action-execution][F01658]") {
  Execution world;
  world.count = 17;
  CHECK_FALSE(ai::TryExecutePlannedActions(world));
  world.count = -1;
  CHECK_FALSE(ai::TryExecutePlannedActions(world));
  world.count = 1;
  world.ranks.at(0).reset();
  CHECK_FALSE(ai::TryExecutePlannedActions(world));
  world.count = 0;
  world.room = false;
  CHECK(ai::TryExecutePlannedActions(world) == 0);
  Value(world.entries.at(2)).SetAction(1);
  CHECK_FALSE(ai::TryExecutePlannedActions(world));
  world.room = true;
  world.special.at(0) = 1;
  CHECK_FALSE(ai::TryExecutePlannedActions(world));
  CHECK_FALSE(ai::TryExecuteCheckedLivePlannedActions(nullptr));
}

TEST_CASE("Decision budget is signed strict and cached only within budget",
          "[action-execution][F01659]") {
  Decisions world;
  world.next.at(1) = 0;
  world.elapsed = -1;
  world.budget = -2;
  CHECK(ai::UpdateActorDecisions(world));
  CHECK(world.cached == -1);
  CHECK(world.calls == std::vector<std::string>{"elapsed:1", "command17:1"});
  world.calls.clear();
  world.budget = -1;
  CHECK(ai::UpdateActorDecisions(world));
  CHECK(world.calls == std::vector<std::string>{"elapsed:1", "reset11:1",
                                                "dispatch:1", "locomotion:1"});
  CHECK(world.directions == std::array<unsigned, 3>{0, 34, 56});
}

TEST_CASE("Decisions skip the head and restricted actors",
          "[action-execution][F01659]") {
  Decisions world;
  world.states.at(1).restricted = true;
  CHECK(ai::UpdateActorDecisions(world));
  CHECK(world.calls.front() == "elapsed:2");
  CHECK(world.calls.size() == 4);
  world.head = 0;
  CHECK_FALSE(ai::UpdateActorDecisions(world));
}

TEST_CASE("Decision callbacks change the current state and next node",
          "[action-execution][F01659]") {
  Decisions world;
  world.callback = [&](const std::string& call) {
    if (call == "elapsed") {
      world.states.at(1).restricted = true;
      world.states.at(1).partial = true;
      world.next.at(1) = 0;
    }
  };
  CHECK(ai::UpdateActorDecisions(world));
  CHECK(world.calls == std::vector<std::string>{"elapsed:1", "execute:1"});
  CHECK(world.distances.at(1) == 12836);
}

TEST_CASE("Partial decision uses record height but independent battle position",
          "[action-execution][F01659]") {
  Decisions world;
  world.next.at(1) = 0;
  world.states.at(1).restricted = true;
  world.states.at(1).partial = true;
  Value(world.player) = {.has_ai = true,
                         .moving = true,
                         .y = 100,
                         .point_height = 77,
                         .body_height = 30};
  CHECK(ai::UpdateActorDecisions(world));
  CHECK(world.distances.at(1) == 11000);
  Value(world.player).y = 0;
  CHECK(ai::UpdateActorDecisions(world));
  CHECK(world.distances.at(1) == 20309);
  world.player.reset();
  CHECK_FALSE(ai::UpdateActorDecisions(world));
  world.player = ai::DecisionPlayer{};
  world.battle_position.reset();
  CHECK_FALSE(ai::UpdateActorDecisions(world));
}

TEST_CASE("Decision lists reject missing states invalid IDs and cycles",
          "[action-execution][F01659]") {
  Decisions world;
  world.present.at(1) = false;
  CHECK_FALSE(ai::UpdateActorDecisions(world));
  world.present.at(1) = true;
  world.ids.at(1) = -1;
  CHECK_FALSE(ai::UpdateActorDecisions(world));
  world.ids.at(1) = 16;
  CHECK_FALSE(ai::UpdateActorDecisions(world));
  world.ids.at(1) = 1;
  world.next.at(1) = 2;
  CHECK_FALSE(ai::UpdateActorDecisions(world));
  world.next.at(1) = 0;
  world.states.at(1).tile_x = 32;
  CHECK_FALSE(ai::UpdateActorDecisions(world));
  world.states.at(1).tile_x = 0;
  world.flags.reset();
  CHECK_FALSE(ai::UpdateActorDecisions(world));
}

TEST_CASE("Live movement reset preserves all bytes outside original stores",
          "[action-execution][F01659]") {
  Battle_Ai_ActorAiState state{};
  state.opaque1 = 7;
  state.opaqueD = 9;
  state.opaqueE = 11;
  state.opaqueF = 13;
  state.actor = reinterpret_cast<Battle_Ai_EngagementData*>(&state);
  state.currentCandidate = &state.currentChoice;
  state.decisionDelay = 99;
  auto expected = state;
  auto bytes = std::bit_cast<std::array<std::byte, sizeof(state)>>(expected);
  for (std::size_t i = 0; i < 14; ++i) bytes.at(i) = std::byte{};
  expected = std::bit_cast<Battle_Ai_ActorAiState>(bytes);
  expected.movementTarget = 16;
  expected.savedDestination.word = UINT32_MAX;
  expected.ground_area = 11;
  expected.reserved154Area = 11;
  ai::ResetLiveDecisionMovement(state, 11);
  CHECK(std::bit_cast<std::array<std::byte, sizeof(state)>>(state) ==
        std::bit_cast<std::array<std::byte, sizeof(state)>>(expected));
  CHECK(state.actor == reinterpret_cast<Battle_Ai_EngagementData*>(&state));
  CHECK(state.currentCandidate == &state.currentChoice);
  CHECK(state.decisionDelay == 99);
}

TEST_CASE("Action execution module bindings use typed live entries",
          "[action-execution][F01657][F01658][F01659]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [&](std::uint32_t address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  CHECK(resolve(0x800DAC80) ==
        reinterpret_cast<void*>(&ai::TryExecuteLiveActionCandidate));
  CHECK(resolve(0x800DAD9C) ==
        reinterpret_cast<void*>(&ai::TryExecuteLivePlannedActions));
  CHECK(resolve(0x800DAED0) ==
        reinterpret_cast<void*>(&ai::UpdateLiveActorDecisions));
}

TEST_CASE("Live partial decisions keep player record and battle model distinct",
          "[action-execution][F01659][live]") {
  LiveDecisionFixture fixture;
  fixture.model.position = {100, 20, 30, 0};
  fixture.record.instance.position = {900, 100, 800, 0};
  fixture.record.instance.locomotionState = 1;
  fixture.player.bodyHeight = 30;
  fixture.player.pointHeight = 77;
  CHECK(ai::UpdateCheckedLiveActorDecisions());
  CHECK(fixture.state.distancesSquared[0] == 11000);
  fixture.record.instance.position.vy = 0;
  CHECK(ai::UpdateCheckedLiveActorDecisions());
  CHECK(fixture.state.distancesSquared[0] == 20309);
  Battle_Ai_actorAiStates[0] = nullptr;
  Actors_actorRecords[0] = nullptr;
  CHECK(ai::UpdateCheckedLiveActorDecisions());
  CHECK(fixture.state.distancesSquared[0] == 12836);
}

TEST_CASE("Live decisions diagnose missing listed states and player records",
          "[action-execution][F01659][live]") {
  LiveDecisionFixture fixture;
  Battle_Ai_FrameState()->actors[1] = nullptr;
  CHECK_FALSE(ai::UpdateCheckedLiveActorDecisions());
  Battle_Ai_FrameState()->actors[1] = &fixture.state;
  Actors_actorRecords[0] = nullptr;
  CHECK_FALSE(ai::UpdateCheckedLiveActorDecisions());
  Actors_actorRecords[0] = &fixture.record;
  fixture.head.model = nullptr;
  CHECK_FALSE(ai::UpdateCheckedLiveActorDecisions());
  fixture.actor.id = 16;
  CHECK_FALSE(ai::UpdateCheckedLiveActorDecisions());
  Battle_actors[0] = nullptr;
  CHECK_FALSE(ai::UpdateCheckedLiveActorDecisions());
}
