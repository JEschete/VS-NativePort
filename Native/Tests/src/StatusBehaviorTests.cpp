#include <algorithm>
#include <array>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/StatusEffects.h"
#include "Core/Random.h"
}

namespace {
struct ConditionFixture {
  Battle_Actor* saved_actor = Battle_actors[0];
  Actors_ActorRecord* saved_record = Actors_actorRecords[0];
  Battle_Ai_ActorAiState* saved_ai = Battle_Ai_actorAiStates[0];
  Battle_ActionState* saved_action = Battle_actionState;
  decltype(Battle_floatingLabels) saved_labels = Battle_floatingLabels;
  decltype(Battle_floatingLabelCount) saved_label_count =
      Battle_floatingLabelCount;
  decltype(std::to_array(Battle_statusEffectParams)) saved_params =
      std::to_array(Battle_statusEffectParams);
  decltype(std::to_array(Battle_actionDefinitions)) saved_definitions =
      std::to_array(Battle_actionDefinitions);
  decltype(std::to_array(Core_randomShuffleTable)) saved_random =
      std::to_array(Core_randomShuffleTable);
  int saved_random_index = Core_randomShuffleIndex;
  Battle_ActorStats stats{};
  Battle_Actor actor{};
  Actors_ActorRecord record{};
  Battle_Ai_ActorAiState ai{};
  Battle_ActionState action{};
  Battle_FloatingLabel labels[32]{};

  ConditionFixture() {
    Battle_actors[0] = &actor;
    actor.stats = &stats;
    actor.weaponDrawn = 1;
    Actors_actorRecords[0] = &record;
    Battle_Ai_actorAiStates[0] = &ai;
    Battle_actionState = &action;
    Battle_floatingLabels = &labels;
    Battle_floatingLabelCount = 0;
    std::fill_n(Core_randomShuffleTable, saved_random.size(), 0);
    Core_randomShuffleIndex = 0;
    InitializeStats();
    InitializePolicies();
  }

  ~ConditionFixture() {
    Battle_actors[0] = saved_actor;
    Actors_actorRecords[0] = saved_record;
    Battle_Ai_actorAiStates[0] = saved_ai;
    Battle_actionState = saved_action;
    Battle_floatingLabels = saved_labels;
    Battle_floatingLabelCount = saved_label_count;
    std::copy(saved_params.begin(), saved_params.end(),
              Battle_statusEffectParams);
    std::copy(saved_definitions.begin(), saved_definitions.end(),
              Battle_actionDefinitions);
    std::copy(saved_random.begin(), saved_random.end(),
              Core_randomShuffleTable);
    Core_randomShuffleIndex = saved_random_index;
  }

  void InitializeStats() {
    stats.currentHp = stats.maxHp = 1000;
    stats.currentMp = stats.maxMp = 999;
    stats.strength = stats.totalStrength = 101;
    stats.intelligence = stats.totalIntelligence = 111;
    stats.agility = stats.totalAgility = 121;
    stats.walkSpeed = stats.baseWalkSpeed = 8;
    stats.runSpeed = stats.baseRunSpeed = 12;
    stats.mobilityLimb0 = 4;
    stats.mobilityLimb1 = 5;
    for (auto& limb : stats.limbs) limb.hp = limb.maxHp = 100;
    for (int id : {97, 141, 224}) {
      Battle_actionDefinitions[id] = {};
      Battle_actionDefinitions[id].unlocked = 1;
      Battle_actionDefinitions[id].type = Battle_ActionType_Spell;
      Battle_actionDefinitions[id].cost = 10;
    }
    Battle_actionDefinitions[141].type = Battle_ActionType_BreakArt;
  }

  void InitializePolicies() {
    // Resident status parameters, G01906 (SLUS file offset 0x3F6CC).
    std::fill_n(Battle_statusEffectParams, saved_params.size(),
                Battle_StatusEffectParams{});
    Battle_statusEffectParams[4] = {0x10, 0, 0, 0x800, 0};
    for (int id = 5; id <= 10; ++id) {
      Battle_statusEffectParams[id] = {1 << id, 15, 0, 0x7E0, 0x10000};
    }
    Battle_statusEffectParams[11] = {0x800, 20, 0, 0x800, 0x8010};
    Battle_statusEffectParams[12] = {0x1000, 0, 0, 0x1000, 0};
    Battle_statusEffectParams[13] = {0x2000, 15, 0, 0x2000, 0};
    Battle_statusEffectParams[14] = {0x4000, 0, 2, 0x20000, 0};
    Battle_statusEffectParams[15] = {0x8000, 15, 0, 0x8800, 0x10};
    Battle_statusEffectParams[16] = {0x10000, 0, 0, 0x7E0, 0x10000};
    Battle_statusEffectParams[17] = {0x20000, 0, 5, 0, 0x4000};
    Battle_statusEffectParams[19] = {0x80000, 15, 0, 0x1FF80000, 0};
  }

  bool Apply(int effect) {
    Battle_HitResult hit{};
    if (Battle_QueueApplicableStatusChanges(effect, &hit) == 0) return false;
    Battle_RemoveActorStatuses(hit.inflictStatuses, &stats);
    Battle_InflictActorStatuses(hit.removeStatuses, &stats);
    return true;
  }

  void Cure(int effect) {
    Battle_HitResult hit{};
    REQUIRE(Battle_QueueStatusRemovalMask(effect, &hit) != 0);
    Battle_RemoveActorStatuses(hit.inflictStatuses, &stats);
  }

  void Tick(int count) {
    for (int index = 0; index < count; ++index) {
      Battle_UpdateActorStatusTimers(&stats);
    }
  }

  void CheckBaseAttributes() const {
    CHECK(stats.strength == 101);
    CHECK(stats.intelligence == 111);
    CHECK(stats.agility == 121);
  }

  void CheckMovement(int walk, int run) const {
    CHECK(stats.walkSpeed == walk);
    CHECK(stats.runSpeed == run);
    CHECK(ai.walkStep == walk);
    CHECK(ai.runStep == run);
  }

  static void CheckActionRestrictions(int restricted_action = -1) {
    for (int action_id : {97, 141, 224}) {
      CAPTURE(action_id);
      CHECK(Battle_ActorActionRestrictionFlags(0, action_id) ==
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
    CHECK(stats.statusTimers[0] == 0);
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
  CHECK(fixture.stats.walkSpeed == 4);
  CHECK_FALSE(fixture.Apply(15));
  fixture.Cure(4);
  CHECK(fixture.stats.limbs[4].hp == 2);
  CHECK(fixture.stats.limbs[5].hp == 2);
  CHECK(fixture.stats.walkSpeed == 8);
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
  fixture.stats.currentHp = 100;
  REQUIRE(fixture.Apply(14));
  CHECK_FALSE(fixture.Apply(17));
  fixture.Tick(1);
  CHECK(fixture.stats.currentHp == 100);
  fixture.Tick(1);
  CHECK(fixture.stats.currentHp >= 90);
  CHECK(fixture.stats.currentHp <= 95);
  CHECK(fixture.stats.statusTimers[5] == 2);
  fixture.Cure(14);
  const auto hp = fixture.stats.currentHp;
  fixture.Tick(4);
  CHECK(fixture.stats.currentHp == hp);
}

TEST_CASE("Poison can exhaust the last point of HP",
          "[combat][status][mechanics]") {
  ConditionFixture fixture;
  fixture.stats.currentHp = 1;
  REQUIRE(fixture.Apply(14));
  fixture.Tick(2);
  CHECK(fixture.stats.currentHp == 0);
}

TEST_CASE("Regeneration restores HP and limbs and is cancelled by poison",
          "[combat][status][mechanics]") {
  ConditionFixture fixture;
  fixture.stats.currentHp = 50;
  fixture.stats.limbs[0].hp = 10;
  REQUIRE(fixture.Apply(17));
  fixture.Tick(4);
  CHECK(fixture.stats.currentHp == 50);
  fixture.Tick(1);
  CHECK(fixture.stats.currentHp >= 98);
  CHECK(fixture.stats.currentHp <= 102);
  CHECK(fixture.stats.limbs[0].hp == 16);
  fixture.stats.currentHp = 999;
  fixture.Tick(5);
  CHECK(fixture.stats.currentHp == 1000);
  REQUIRE(fixture.Apply(14));
  CHECK(fixture.stats.statuses == 0x4000);
}

TEST_CASE("Equipment down reduces equipped strength and restores baseline",
          "[combat][status][mechanics]") {
  ConditionFixture fixture;
  auto& stats = fixture.stats;
  stats.weapon.blade.id = stats.shield.base.id = stats.limbs[0].armor.armor.id =
      1;
  stats.weapon.currentStr = 20;
  stats.weapon.baseStr = 20;
  stats.weapon.currentInt = 20;
  stats.weapon.baseInt = 20;
  stats.shield.currentStr = 30;
  stats.shield.baseStr = 30;
  stats.limbs[0].armor.currentStr = 40;
  stats.limbs[0].armor.baseStr = 40;
  REQUIRE(fixture.Apply(19));
  CHECK(stats.weapon.currentStr == 11);
  CHECK(stats.shield.currentStr == 21);
  CHECK(stats.limbs[0].armor.currentStr == 31);
  fixture.Tick(15);
  CHECK(stats.statuses == 0);
  CHECK(stats.weapon.currentStr == 20);
  CHECK(stats.shield.currentStr == 30);
  CHECK(stats.limbs[0].armor.currentStr == 40);
}

TEST_CASE("Ailment policy respects immunity and the training dummy exclusion",
          "[combat][status][mechanics]") {
  ConditionFixture fixture;
  fixture.stats.statusImmunities = 0x4000;
  CHECK_FALSE(fixture.Apply(14));
  CHECK(fixture.stats.statuses == 0);
  fixture.stats.statusImmunities = 0;
  fixture.stats.actorId = 128;
  CHECK_FALSE(fixture.Apply(14));
  CHECK(fixture.stats.statuses == 0);
}
