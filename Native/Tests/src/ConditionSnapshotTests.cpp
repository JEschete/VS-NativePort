// Hand-maintained tests for the typed AI condition snapshot (#247).
#include <array>
#include <cstring>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/ConditionSnapshot.hpp"
#include "Battle/Ai/ConditionSnapshotBinding.hpp"
#include "Content/ModuleData.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Actor.h"
#include "Battle/Damage.h"
}

namespace {
struct StoredCondition {
  int actor_id;
  VS::Battle::Ai::ActorCondition condition;
};

class RecordingTable final : public VS::Battle::Ai::ActorConditionTable {
 public:
  bool StoreActorCondition(
      int actor_id, const VS::Battle::Ai::ActorCondition& condition) override {
    stored.push_back({actor_id, condition});
    return true;
  }
  std::vector<StoredCondition> stored;
};
}  // namespace

TEST_CASE("Condition snapshots store each reading in list order",
          "[ai][condition][condition-snapshot]") {
  using VS::Battle::Ai::ActorCondition;
  const ActorCondition wounded{20, 100, 15, 60, 0x1040u, 10, 40, 30, 60};
  const ActorCondition fresh{120, 120, 0, 0, 0x1001u, 0, 0, 0, 0};
  const std::array<VS::Battle::Ai::ActorConditionReading, 2> readings{{
      {5, fresh},
      {2, wounded},
  }};
  RecordingTable table;
  VS::Battle::Ai::CaptureActorConditionTable(readings, table);
  REQUIRE(table.stored.size() == 2);
  CHECK(table.stored[0].actor_id == 5);
  CHECK(table.stored[0].condition == fresh);
  CHECK(table.stored[1].actor_id == 2);
  CHECK(table.stored[1].condition == wounded);
}

TEST_CASE("An empty actor list leaves the condition table untouched",
          "[ai][condition][condition-snapshot]") {
  RecordingTable table;
  VS::Battle::Ai::CaptureActorConditionTable({}, table);
  CHECK(table.stored.empty());
}

TEST_CASE("The promoted condition snapshot has no original module address",
          "[ai][condition][condition-snapshot]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800DC424u) == nullptr);
}

TEST_CASE("The planner-row binding writes only listed actors' rows",
          "[ai][condition][condition-snapshot]") {
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  actor.id = VS::Battle::Ai::kBattleActorSlotCount - 1;
  actor.stats = &stats;
  stats.currentHp = 7;
  stats.maxHp = 9;
  stats.statuses = 1 << 3;
  stats.shield.maxDp = 50;
  std::array<Battle_Ai_ActionTiming, VS::Battle::Ai::kBattleActorSlotCount>
      rows{};
  std::memset(rows.data(), 0xA5, sizeof rows);
  Battle_Actor* saved = Battle_actors[0];
  Battle_actors[0] = &actor;
  VS::Battle::Ai::CaptureBattleActorConditions(rows);
  Battle_actors[0] = saved;
  const Battle_Ai_ActionTiming& row = rows.back();
  CHECK(row.opaque0Bit0 == 15u);
  CHECK(row.reserved0Bit4 == 0u);
  CHECK(row.opaque4 == 7);
  CHECK(row.opaque6 == 9);
  CHECK(row.opaqueC == 8u);
  CHECK(row.opaque16 == 50);
  CHECK(rows[0].opaqueC == 0xA5A5A5A5u);
}
