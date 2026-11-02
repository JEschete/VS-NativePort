#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/Relations.hpp"

namespace {
using VS::Battle::Ai::kRelationActorSlots;

class FakeRelationTable final : public VS::Battle::Ai::ActorRelationTable {
 public:
  std::vector<std::uint8_t> room;
  std::array<std::uint8_t, kRelationActorSlots> categories{};
  std::array<std::array<unsigned, kRelationActorSlots>, kRelationActorSlots>
      classes{};
  std::vector<int> sources;

  [[nodiscard]] std::span<const std::uint8_t> RoomActors() const override {
    return room;
  }
  [[nodiscard]] std::uint8_t Category(int actor) const override {
    return categories.at(actor);
  }
  [[nodiscard]] unsigned Classification(int source, int target) const override {
    return classes.at(source).at(target);
  }
  void SetClassification(int source, int target, unsigned value) override {
    if (sources.empty() || sources.back() != source) sources.push_back(source);
    classes.at(source).at(target) = value;
  }
};

class FakeAllianceActor final : public VS::Battle::Ai::AshleyAllianceActor {
 public:
  std::uint8_t count = 0;
  bool active = false;
  bool recovery_disabled = false;
  std::vector<int> rolls;
  std::vector<std::string> calls;

  [[nodiscard]] std::uint8_t ReactionCount() const override { return count; }
  void SetReactionCount(std::uint8_t value) override { count = value; }
  [[nodiscard]] bool AllianceActive() const override { return active; }
  void SetAllianceActive(bool value) override { active = value; }
  void SetAwarenessIndicator(int marker) override {
    calls.push_back("marker " + std::to_string(marker));
  }
  void RecomputeRelations() override { calls.emplace_back("relations"); }
  void SaveDecisionAndApproach() override { calls.emplace_back("approach"); }
  void RestoreSavedDecision() override { calls.emplace_back("restore"); }
  [[nodiscard]] bool RecoveryDisabled() const override {
    return recovery_disabled;
  }
  void SetTemporaryAwareness(int marker, int frames) override {
    calls.push_back("awareness " + std::to_string(marker) + " " +
                    std::to_string(frames));
  }
  void QueueReaction(int command, int range) override {
    calls.push_back("reaction " + std::to_string(command) + " " +
                    std::to_string(range));
  }
  [[nodiscard]] int Roll() override {
    REQUIRE(!rolls.empty());
    const int roll = rolls.front();
    rolls.erase(rolls.begin());
    return roll;
  }
};

class FakeSavedActors final : public VS::Battle::Ai::SavedRelationActors {
 public:
  std::vector<std::uint8_t> room;
  std::array<std::int16_t, kRelationActorSlots> options{};
  std::array<VS::Battle::Ai::SavedAshleyRelation, kRelationActorSlots>
      restored{};
  std::vector<int> restored_order;
  std::vector<int> allied;

  [[nodiscard]] std::span<const std::uint8_t> RoomActors() const override {
    return room;
  }
  [[nodiscard]] std::int16_t SavedOptions(int actor) const override {
    return options.at(actor);
  }
  void RestoreAshleyRelation(
      int actor, const VS::Battle::Ai::SavedAshleyRelation& relation) override {
    restored.at(actor) = relation;
    restored_order.push_back(actor);
  }
  void BeginForcedAlliance(int actor) override { allied.push_back(actor); }
};

FakeRelationTable ThreeActorRoom() {
  FakeRelationTable table;
  table.room = {0, 2, 5};
  table.categories[0] = 3;
  table.categories[2] = 0;
  table.categories[5] = 2;
  return table;
}

// The source's classifications of actors 0, 2 and 5.
std::array<unsigned, 3> RoomRow(const FakeRelationTable& table, int source) {
  const auto& row = table.classes.at(source);
  return {row[0], row[2], row[5]};
}
}  // namespace

TEST_CASE("AI relations classify the room and spread to unclassified actors",
          "[ai][relations]") {
  FakeRelationTable table = ThreeActorRoom();
  REQUIRE(VS::Battle::Ai::InitializeActorAiRelations(table, 2));
  CHECK(table.sources == std::vector<int>{2, 0, 5});
  CHECK(RoomRow(table, 2) == std::array<unsigned, 3>{3, 2, 2});
  CHECK(RoomRow(table, 0) == std::array<unsigned, 3>{2, 3, 3});
  CHECK(RoomRow(table, 5) == std::array<unsigned, 3>{1, 2, 2});
}

TEST_CASE("AI relations skip actors that already classify the source",
          "[ai][relations]") {
  FakeRelationTable table = ThreeActorRoom();
  table.classes[0][2] = 1;
  table.classes[0][5] = 1;
  REQUIRE(VS::Battle::Ai::InitializeActorAiRelations(table, 2));
  CHECK(table.sources == std::vector<int>{2, 5});
  CHECK(table.classes[0][0] == 0);
  CHECK(table.classes[0][2] == 1);
}

TEST_CASE("AI relations refuse a source without a valid category",
          "[ai][relations]") {
  FakeRelationTable table = ThreeActorRoom();
  table.categories[5] = 4;
  CHECK_FALSE(VS::Battle::Ai::InitializeActorAiRelations(table, 2));
  CHECK(table.sources == std::vector<int>{2, 0});
  CHECK(table.classes[5][0] == 0);
}

TEST_CASE("Ashley alliance needs two favourable draws", "[ai][relations]") {
  FakeAllianceActor actor;
  actor.rolls = {0x4C, 0x14B, 0x80};
  VS::Battle::Ai::TryBeginAshleyAlliance(actor, false);
  CHECK(actor.count == 0);
  VS::Battle::Ai::TryBeginAshleyAlliance(actor, false);
  CHECK(actor.count == 1);
  CHECK_FALSE(actor.active);
  CHECK(actor.calls.empty());
  VS::Battle::Ai::TryBeginAshleyAlliance(actor, false);
  CHECK(actor.count == 2);
  CHECK(actor.active);
  CHECK(actor.calls == std::vector<std::string>{"approach", "marker 128"});
}

TEST_CASE("Ashley alliance compares the untruncated reaction count",
          "[ai][relations]") {
  FakeAllianceActor actor;
  actor.count = 0xFF;
  actor.rolls = {0};
  VS::Battle::Ai::TryBeginAshleyAlliance(actor, false);
  CHECK(actor.count == 0);
  CHECK(actor.active);
}

TEST_CASE("Forced Ashley alliance draws nothing and keeps the count",
          "[ai][relations]") {
  FakeAllianceActor actor;
  actor.count = 1;
  VS::Battle::Ai::TryBeginAshleyAlliance(actor, true);
  CHECK(actor.count == 1);
  CHECK(actor.active);
  CHECK(actor.calls == std::vector<std::string>{"approach", "marker 128"});
}

TEST_CASE("Ending an Ashley alliance restores behaviour and queues recovery",
          "[ai][relations]") {
  FakeAllianceActor actor;
  actor.count = 1;
  VS::Battle::Ai::EndAshleyAlliance(actor);
  CHECK(actor.count == 0);
  CHECK(actor.calls.empty());

  actor.active = true;
  actor.count = 3;
  VS::Battle::Ai::EndAshleyAlliance(actor);
  CHECK(actor.count == 0);
  CHECK_FALSE(actor.active);
  CHECK(actor.calls == std::vector<std::string>{"marker 64", "relations",
                                                "restore", "awareness 6 40",
                                                "reaction 2 128"});

  actor.calls.clear();
  actor.active = true;
  actor.recovery_disabled = true;
  VS::Battle::Ai::EndAshleyAlliance(actor);
  CHECK(actor.calls ==
        std::vector<std::string>{"marker 64", "relations", "restore"});
}

TEST_CASE("Saved relation options decode the Ashley relation",
          "[ai][relations]") {
  const auto relation = VS::Battle::Ai::DecodeSavedAshleyRelation(0x1235);
  CHECK(relation.allied);
  CHECK(relation.classification == ((0x1235 >> 1) & 3));
  CHECK(relation.previous_classification == ((0x1235 >> 3) & 3));
  CHECK(relation.classification_timer == ((0x1235 >> 6) & 0x7F));
  CHECK_FALSE(VS::Battle::Ai::DecodeSavedAshleyRelation(0x1234).allied);
}

TEST_CASE("Saved relations restore only actors with saved options",
          "[ai][relations]") {
  FakeSavedActors actors;
  actors.room = {0, 3, 7};
  actors.options[0] = -1;
  actors.options[3] = 0x1234;
  actors.options[7] = 0x0001;
  VS::Battle::Ai::ApplySavedActorAiRelations(actors);
  CHECK(actors.restored_order == std::vector<int>{3, 7});
  CHECK(actors.restored[3].classification == 2);
  CHECK(actors.restored[3].previous_classification == 2);
  CHECK(actors.restored[3].classification_timer == 0x48);
  CHECK(actors.allied == std::vector<int>{7});
}
