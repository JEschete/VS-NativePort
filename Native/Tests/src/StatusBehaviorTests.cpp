#include <algorithm>
#include <array>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorRecord.hpp"
#include "Battle/ActionCostsLive.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionResolution.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiState.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/StatusEffects.hpp"
#include "Battle/StatusLifecycleLive.hpp"
#include "Battle/StatusQueueLive.hpp"
#include "Core/RandomLive.hpp"
#include "Legacy/Psx.hpp"
#include "TestSupport/StatusRoot.hpp"

using VS::Battle::ActionState;
using VS::Battle::kActionTypeBreakArt;
using VS::Battle::kActionTypeSpell;

namespace {
struct ConditionFixture {
  VS::Battle::Actor* saved_actor = VS::Battle::actors[0];
  VS::Actors::ActorRecord* saved_record = VS::Actors::actorRecords[0];
  VS::Battle::Ai::ActorAiState* saved_ai = VS::Battle::Ai::actorAiStates[0];
  ActionState* saved_action = VS::Battle::actionState;
  decltype(VS::Battle::floatingLabels) saved_labels =
      VS::Battle::floatingLabels;
  decltype(VS::Battle::floatingLabelCount) saved_label_count =
      VS::Battle::floatingLabelCount;
  VS::test_support::StatusRoot catalog;
  decltype(VS::Battle::actionDefinitions) saved_definitions =
      VS::Battle::actionDefinitions;
  decltype(std::to_array(VS::Core::randomShuffleTable)) saved_random =
      std::to_array(VS::Core::randomShuffleTable);
  int saved_random_index = VS::Core::randomShuffleIndex;
  VS::Battle::ActorStats stats{};
  VS::Battle::Actor actor{};
  VS::Actors::ActorRecord record{};
  VS::Battle::Ai::ActorAiState ai{};
  ActionState action{};
  VS::Battle::FloatingLabelSlots labels{};

  ConditionFixture() {
    VS::Battle::actors[0] = &actor;
    actor.stats = &stats;
    actor.weapon_drawn = 1;
    VS::Actors::actorRecords[0] = &record;
    VS::Battle::Ai::actorAiStates[0] = &ai;
    VS::Battle::actionState = &action;
    VS::Battle::floatingLabels = &labels;
    VS::Battle::floatingLabelCount = 0;
    std::fill_n(VS::Core::randomShuffleTable, saved_random.size(), 0);
    VS::Core::randomShuffleIndex = 0;
    InitializeStats();
    InitializePolicies();
  }

  ~ConditionFixture() {
    VS::Battle::actors[0] = saved_actor;
    VS::Actors::actorRecords[0] = saved_record;
    VS::Battle::Ai::actorAiStates[0] = saved_ai;
    VS::Battle::actionState = saved_action;
    VS::Battle::floatingLabels = saved_labels;
    VS::Battle::floatingLabelCount = saved_label_count;
    std::copy(saved_definitions.begin(), saved_definitions.end(),
              VS::Battle::actionDefinitions.data());
    std::copy(saved_random.begin(), saved_random.end(),
              VS::Core::randomShuffleTable);
    VS::Core::randomShuffleIndex = saved_random_index;
  }

  void InitializeStats() {
    stats.current_hp = stats.max_hp = 1000;
    stats.current_mp = stats.max_mp = 999;
    stats.strength = stats.total_strength = 101;
    stats.intelligence = stats.total_intelligence = 111;
    stats.agility = stats.total_agility = 121;
    stats.walk_speed = stats.base_walk_speed = 8;
    stats.run_speed = stats.base_run_speed = 12;
    stats.mobility_limb0 = 4;
    stats.mobility_limb1 = 5;
    for (auto& limb : stats.limbs) limb.hp = limb.max_hp = 100;
    for (int id : {97, 141, 224}) {
      VS::Battle::actionDefinitions[id] = {};
      VS::Battle::actionDefinitions[id].unlocked = 1;
      VS::Battle::actionDefinitions[id].type = kActionTypeSpell;
      VS::Battle::actionDefinitions[id].cost = 10;
    }
    VS::Battle::actionDefinitions[141].type = kActionTypeBreakArt;
  }

  void InitializePolicies() {
    auto& rules = catalog.definitions.rules;
    rules[4] = {0x10, 0x800, 0};
    for (int id = 5; id <= 10; ++id) {
      rules[id] = {1U << id, 0x7E0, 0x10000};
    }
    rules[11] = {0x800, 0x800, 0x8010};
    rules[12] = {0x1000, 0x1000, 0};
    rules[13] = {0x2000, 0x2000, 0};
    rules[14] = {0x4000, 0x20000, 0};
    rules[15] = {0x8000, 0x8800, 0x10};
    rules[16] = {0x10000, 0x7E0, 0x10000};
    rules[17] = {0x20000, 0, 0x4000};
    rules[19] = {0x80000, 0x1FF80000, 0};
    catalog.Publish();
  }

  bool Apply(int effect) {
    VS::Battle::HitResult hit{};
    if (VS::Battle::QueueLiveApplicableStatusChanges(effect, &hit) == 0) {
      return false;
    }
    VS::Battle::RemoveLiveActorStatuses(hit.inflict_statuses, &stats);
    VS::Battle::InflictLiveActorStatuses(hit.remove_statuses, &stats);
    return true;
  }

  void Cure(int effect) {
    VS::Battle::HitResult hit{};
    REQUIRE(VS::Battle::QueueLiveStatusRemovalMask(effect, &hit) != 0);
    VS::Battle::RemoveLiveActorStatuses(hit.inflict_statuses, &stats);
  }

  void Tick(int count) {
    for (int index = 0; index < count; ++index) {
      VS::Battle::UpdateLiveActorStatusTimers(&stats);
    }
  }

  void CheckBaseAttributes() const {
    CHECK(stats.strength == 101);
    CHECK(stats.intelligence == 111);
    CHECK(stats.agility == 121);
  }

  void CheckMovement(int walk, int run) const {
    CHECK(stats.walk_speed == walk);
    CHECK(stats.run_speed == run);
    CHECK(ai.walk_step == walk);
    CHECK(ai.run_step == run);
  }

  static void CheckActionRestrictions(int restricted_action = -1) {
    for (int action_id : {97, 141, 224}) {
      CAPTURE(action_id);
      CHECK(VS::Battle::LiveActorActionRestrictionFlags(0, action_id) ==
            (action_id == restricted_action ? 4 : 0));
    }
  }
};
}  // namespace

TEST_CASE("Attribute ailments change actual stats and expire or cure",
          "[combat][status][mechanics]") {
  for (const int effect : {5, 7, 9}) {
    CAPTURE(effect);
    ConditionFixture fixture;
    auto& stats = fixture.stats;
    REQUIRE(fixture.Apply(effect));
    CHECK(stats.strength == (effect == 5 ? 91 : 101));
    CHECK(stats.intelligence == (effect == 7 ? 100 : 111));
    CHECK(stats.agility == (effect == 9 ? 109 : 121));
    fixture.Tick(14);
    CHECK(stats.statuses != 0);
    fixture.Tick(1);
    CHECK(stats.statuses == 0);
    fixture.CheckBaseAttributes();
    REQUIRE(fixture.Apply(effect));
    fixture.Cure(effect);
    CHECK(stats.status_timers[0] == 0);
    fixture.CheckBaseAttributes();
  }
}

TEST_CASE("Curse replaces attribute buffs and persists until removal",
          "[combat][status][mechanics]") {
  ConditionFixture fixture;
  REQUIRE(fixture.Apply(6));
  REQUIRE(fixture.Apply(16));
  CHECK(fixture.stats.strength == 91);
  CHECK(fixture.stats.intelligence == 100);
  CHECK(fixture.stats.agility == 109);
  CHECK(fixture.stats.statuses == 0x10000);
  CHECK_FALSE(fixture.Apply(6));
  fixture.Tick(30);
  CHECK(fixture.stats.statuses == 0x10000);
  fixture.Cure(16);
  fixture.CheckBaseAttributes();
}

TEST_CASE("Numbness halves movement and cancels Quicken",
          "[combat][status][mechanics]") {
  ConditionFixture fixture;
  REQUIRE(fixture.Apply(11));
  fixture.CheckMovement(10, 15);
  REQUIRE(fixture.Apply(15));
  CHECK(fixture.stats.statuses == 0x8000);
  fixture.CheckMovement(4, 6);
  CHECK_FALSE(fixture.Apply(11));
  fixture.Tick(15);
  CHECK(fixture.stats.statuses == 0);
  fixture.CheckMovement(8, 12);
}

TEST_CASE("Half movement from injured legs clears only when legs recover",
          "[combat][status][mechanics]") {
  ConditionFixture fixture;
  fixture.stats.limbs[4].hp = fixture.stats.limbs[5].hp = 1;
  REQUIRE(fixture.Apply(4));
  fixture.Tick(1);
  CHECK(fixture.stats.statuses == 0x10);
  CHECK(fixture.stats.walk_speed == 4);
  CHECK_FALSE(fixture.Apply(15));
  fixture.Cure(4);
  CHECK(fixture.stats.limbs[4].hp == 2);
  CHECK(fixture.stats.limbs[5].hp == 2);
  CHECK(fixture.stats.walk_speed == 8);
  REQUIRE(fixture.Apply(4));
  fixture.Tick(1);
  CHECK(fixture.stats.statuses == 0);
}

TEST_CASE("Silence and paralysis restrict their actual action classes",
          "[combat][status][mechanics]") {
  ConditionFixture fixture;
  fixture.CheckActionRestrictions();
  REQUIRE(fixture.Apply(12));
  fixture.CheckActionRestrictions(97);
  fixture.Tick(30);
  fixture.CheckActionRestrictions(97);
  fixture.Cure(12);
  REQUIRE(fixture.Apply(13));
  fixture.CheckActionRestrictions(141);
  fixture.Tick(15);
  fixture.CheckActionRestrictions();
}

TEST_CASE("Poison damages HP on its interval and stops after curing",
          "[combat][status][mechanics]") {
  ConditionFixture fixture;
  fixture.stats.current_hp = 100;
  REQUIRE(fixture.Apply(14));
  CHECK_FALSE(fixture.Apply(17));
  fixture.Tick(1);
  CHECK(fixture.stats.current_hp == 100);
  fixture.Tick(1);
  CHECK(fixture.stats.current_hp >= 90);
  CHECK(fixture.stats.current_hp <= 95);
  CHECK(fixture.stats.status_timers[5] == 2);
  fixture.Cure(14);
  const auto hp = fixture.stats.current_hp;
  fixture.Tick(4);
  CHECK(fixture.stats.current_hp == hp);
}

TEST_CASE("Poison can exhaust the last point of HP",
          "[combat][status][mechanics]") {
  ConditionFixture fixture;
  fixture.stats.current_hp = 1;
  REQUIRE(fixture.Apply(14));
  fixture.Tick(2);
  CHECK(fixture.stats.current_hp == 0);
}

TEST_CASE("Regeneration restores HP and limbs and is cancelled by poison",
          "[combat][status][mechanics]") {
  ConditionFixture fixture;
  fixture.stats.current_hp = 50;
  fixture.stats.limbs[0].hp = 10;
  REQUIRE(fixture.Apply(17));
  fixture.Tick(4);
  CHECK(fixture.stats.current_hp == 50);
  fixture.Tick(1);
  CHECK(fixture.stats.current_hp >= 98);
  CHECK(fixture.stats.current_hp <= 102);
  CHECK(fixture.stats.limbs[0].hp == 16);
  fixture.stats.current_hp = 999;
  fixture.Tick(5);
  CHECK(fixture.stats.current_hp == 1000);
  REQUIRE(fixture.Apply(14));
  CHECK(fixture.stats.statuses == 0x4000);
}

TEST_CASE("Equipment down reduces equipped strength and restores baseline",
          "[combat][status][mechanics]") {
  ConditionFixture fixture;
  auto& stats = fixture.stats;
  stats.weapon.blade.id = stats.shield.base.id = stats.limbs[0].armor.armor.id =
      1;
  stats.weapon.attributes.current_strength = 20;
  stats.weapon.attributes.base_strength = 20;
  stats.weapon.attributes.current_intelligence = 20;
  stats.weapon.attributes.base_intelligence = 20;
  stats.shield.attributes.current_strength = 30;
  stats.shield.attributes.base_strength = 30;
  stats.limbs[0].armor.attributes.current_strength = 40;
  stats.limbs[0].armor.attributes.base_strength = 40;
  REQUIRE(fixture.Apply(19));
  CHECK(stats.weapon.attributes.current_strength == 11);
  CHECK(stats.shield.attributes.current_strength == 21);
  CHECK(stats.limbs[0].armor.attributes.current_strength == 31);
  fixture.Tick(15);
  CHECK(stats.statuses == 0);
  CHECK(stats.weapon.attributes.current_strength == 20);
  CHECK(stats.shield.attributes.current_strength == 30);
  CHECK(stats.limbs[0].armor.attributes.current_strength == 40);
}

TEST_CASE("Ailment policy respects immunity and the training dummy exclusion",
          "[combat][status][mechanics]") {
  ConditionFixture fixture;
  fixture.stats.status_immunities = 0x4000;
  CHECK_FALSE(fixture.Apply(14));
  CHECK(fixture.stats.statuses == 0);
  fixture.stats.status_immunities = 0;
  fixture.stats.actor_id = 128;
  CHECK_FALSE(fixture.Apply(14));
  CHECK(fixture.stats.statuses == 0);
}
