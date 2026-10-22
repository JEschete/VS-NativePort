#include <algorithm>
#include <array>
#include <iterator>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Abilities.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Effects/EffectResources.h"
#include "Effects/PurgeTables.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/Ring.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "State/Flags.h"
#include "State/Settings.h"
#include "State/Timers.h"
}

extern "C" int Tests_CombatTargetResults(void);

namespace {
// Original named action definitions, SLUS_010.40 file offset 0x3C1DC.
constexpr std::array<std::pair<int, int>, 28> kAbilityCosts{{
    {22, 1}, {23, 2}, {24, 1}, {25, 2}, {26, 1}, {27, 1}, {28, 2},
    {29, 2}, {30, 1}, {31, 3}, {32, 3}, {34, 3}, {35, 3}, {37, 3},
    {40, 1}, {41, 6}, {42, 2}, {43, 2}, {44, 4}, {45, 4}, {46, 4},
    {47, 4}, {48, 4}, {49, 4}, {50, 4}, {51, 4}, {52, 4}, {53, 6},
}};

struct AbilityFixture {
  decltype(std::to_array(Battle_actionDefinitions)) saved_definitions =
      std::to_array(Battle_actionDefinitions);
  Battle_ActionState* saved_action = Battle_actionState;
  Battle_Actor* saved_character = Battle_characterState;
  std::array<Battle_Actor*, 2> saved_actors{Battle_actors[0], Battle_actors[1]};
  std::array<Actors_ActorRecord*, 2> saved_records{Actors_actorRecords[0],
                                                   Actors_actorRecords[1]};
  State_Settings saved_settings = Input_settings;
  decltype(Input_buttonsPressed) saved_pressed = Input_buttonsPressed;
  decltype(Audio_Seq_unusedScoredata) saved_score = Audio_Seq_unusedScoredata;
  decltype(Battle_floatingLabels) saved_labels = Battle_floatingLabels;
  decltype(Battle_floatingLabelCount) saved_label_count =
      Battle_floatingLabelCount;
  decltype(Battle_abilityInputResult) saved_input_result =
      Battle_abilityInputResult;
  u_int saved_actor_flags = Actors_stateFlagsScratch;
  int saved_tick_speed = State_tickSpeed;
  char saved_difficulty = State_ScriptFlag(1);
  Battle_ActionState action{};
  Battle_Actor actors[2]{};
  Battle_ActorStats stats[2]{};
  Actors_ActorRecord records[2]{};
  Battle_FloatingLabel labels[32]{};

  AbilityFixture() {
    Battle_actionState = &action;
    Battle_characterState = &actors[0];
    for (int index = 0; index < 2; ++index) {
      Battle_actors[index] = &actors[index];
      Actors_actorRecords[index] = &records[index];
      actors[index].id = index;
      actors[index].stats = &stats[index];
      actors[index].roomSpawnIndex = 1;
      stats[index].actorId = index;
      stats[index].currentHp = 600;
      stats[index].currentMp = 120;
      records[index].instance.actionAnimationState = 1;
    }
    std::fill_n(Battle_actionDefinitions, saved_definitions.size(),
                Battle_ActionDefinition{});
    for (const auto& [id, cost] : kAbilityCosts) {
      Battle_actionDefinitions[id].type = Battle_ActionType_Ability;
      Battle_actionDefinitions[id].cost = cost;
      Battle_actionDefinitions[id].unlocked = 1;
    }
    Input_settings = {};
    for (int index = 0; index < 3; ++index) {
      Input_settings.mappedChainAbilities[index] =
          static_cast<char>(22 + index);
      Input_settings.mappedDefenseAbilities[index] =
          static_cast<char>(40 + index);
    }
    Input_buttonsPressed.allButtons = PADRright;
    Battle_floatingLabels = &labels;
    Battle_floatingLabelCount = 0;
    Battle_abilityInputResult = 1;
    State_tickSpeed = 2;
    State_SetScriptFlag(1, 0);
    Prepare();
  }

  ~AbilityFixture() {
    std::copy(saved_definitions.begin(), saved_definitions.end(),
              Battle_actionDefinitions);
    Battle_actionState = saved_action;
    Battle_characterState = saved_character;
    for (int index = 0; index < 2; ++index) {
      Battle_actors[index] = saved_actors[index];
      Actors_actorRecords[index] = saved_records[index];
    }
    Input_settings = saved_settings;
    Input_buttonsPressed = saved_pressed;
    Audio_Seq_unusedScoredata = saved_score;
    Battle_floatingLabels = saved_labels;
    Battle_floatingLabelCount = saved_label_count;
    Battle_abilityInputResult = saved_input_result;
    Actors_stateFlagsScratch = saved_actor_flags;
    State_tickSpeed = saved_tick_speed;
    State_SetScriptFlag(1, saved_difficulty);
  }

  void Prepare(bool defense = false, bool spell = false, unsigned index = 0) {
    action = {};
    action.queueIndex = index;
    auto& request = Request();
    request.actionId = spell ? 99 : 1;
    request.primaryTarget.target.targetActor = defense ? 1 : 0;
    request.targetCount = 1;
    request.targets[0].target.targetActor = defense ? 0 : 1;
    action.abilityWindow = 100;
  }

  Battle_ActionRequest& Request() {
    return action.queue[action.queueIndex & 3];
  }

  void CheckInputResult(int success, int slot) const {
    CHECK(action.abilityInputSucceeded == success);
    CHECK(action.chainAbilitySlot == slot);
    CHECK(action.previousChainInput == slot);
  }
};
}  // namespace

TEST_CASE(
    "Combat sandbox applies matching target HP and MP effects with clamping",
    "[combat][sandbox]") {
  REQUIRE(Tests_CombatTargetResults() == 1);
}

TEST_CASE("Stat item effects bind plugin-local renderers, not EFFPURGE",
          "[combat][items][effects][item-plugin]") {
  const auto count = Effects_effectEntryPointIndices
      [std::size(Effects_effectEntryPointIndices) - 1];
  for (unsigned slot = 0; slot < count; ++slot) {
    CAPTURE(slot);
    CHECK(Effects_effectEntryPoints[slot] != Effects_InvokeAllPluginUpdates);
  }
  CHECK(Effects_effectEntryPoints[Effects_effectEntryPointIndices[198]] ==
        Effects_Plg198_Ring);
  CHECK(Effects_effectEntryPoints[Effects_effectEntryPointIndices[199]] ==
        Effects_Plg198_Ring);
}

TEST_CASE("All battle abilities charge original Risk with chain growth and cap",
          "[combat][abilities][cost]") {
  AbilityFixture fixture;
  // BATTLE.PRG _getActionCost, 146C.c:13075.
  const int step = GENERATE(0, 9, 10, 31);
  const int risk = GENERATE(0, 97, 100);
  for (const auto& [id, base_cost] : kAbilityCosts) {
    CAPTURE(id, step, risk);
    fixture.action.chainStep = step;
    fixture.stats[0].risk = risk;
    const int cost =
        base_cost + (id < 40 && step >= 10 ? (step * step) / 10 : 0);
    const int expected = (1 << 24) + (Battle_ActionType_Ability << 16) + cost;
    CHECK(Battle_ActionCostAndAvailability(id, &fixture.stats[0], 0) ==
          expected);
    CHECK(fixture.stats[0].risk == risk);
    CHECK(Battle_ActionCostAndAvailability(id, &fixture.stats[0], 1) ==
          expected);
    CHECK(fixture.stats[0].risk == std::min(100, risk + cost));
    CHECK(fixture.stats[0].currentHp == 600);
    CHECK(fixture.stats[0].currentMp == 120);
  }
}

TEST_CASE("Ability eligibility requires mapped input",
          "[combat][abilities][eligibility]") {
  AbilityFixture fixture;
  REQUIRE(Battle_CanTriggerBattleAbility(0) == 1);
  REQUIRE(Battle_CanTriggerBattleAbility(1) == 1);

  SECTION("no mapped offensive buttons") {
    std::fill_n(Input_settings.mappedChainAbilities, 3, 0);
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
  SECTION("no mapped defensive buttons") {
    fixture.Prepare(true);
    REQUIRE(Battle_CanTriggerBattleAbility(0) == 1);
    std::fill_n(Input_settings.mappedDefenseAbilities, 3, 0);
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
}

TEST_CASE("Ability eligibility requires a fresh and distinct input",
          "[combat][abilities][eligibility]") {
  AbilityFixture fixture;
  REQUIRE(Battle_CanTriggerBattleAbility(0) == 1);
  REQUIRE(Battle_CanTriggerBattleAbility(1) == 1);
  SECTION("consecutive buttons must differ") {
    fixture.action.previousChainInput = 1;
    CHECK(Battle_CanTriggerBattleAbility(1) == 0);
    CHECK(Battle_CanTriggerBattleAbility(2) == 1);
  }
  SECTION("only one attempt per action") {
    fixture.action.abilityInputAttempted = 1;
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
    CHECK(Battle_CanTriggerBattleAbility(2) == 0);
  }
}

TEST_CASE("Ability eligibility requires real non-dummy actor targets",
          "[combat][abilities][eligibility]") {
  AbilityFixture fixture;
  REQUIRE(Battle_CanTriggerBattleAbility(0) == 1);
  SECTION("source is a position") {
    fixture.Request().primaryTarget.targetsPosition = 1;
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
  SECTION("target is a position") {
    fixture.Request().targets[0].targetsPosition = 1;
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
  SECTION("no target") {
    fixture.Request().targetCount = 0;
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
  SECTION("training dummy") {
    fixture.actors[1].roomSpawnIndex = 0x80;
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
}

TEST_CASE("Ability eligibility requires an appropriate combat relationship",
          "[combat][abilities][eligibility]") {
  AbilityFixture fixture;
  REQUIRE(Battle_CanTriggerBattleAbility(0) == 1);
  SECTION("self target") {
    fixture.Request().targets[0].target.targetActor = 0;
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
  SECTION("enemy action does not target Ashley") {
    fixture.Prepare(true);
    fixture.Request().targets[0].target.targetActor = 1;
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
  SECTION("Ashley cannot chain from magic") {
    fixture.Request().actionId = 99;
    CHECK(Battle_CanTriggerBattleAbility(0) == 0);
  }
  SECTION("defenses cannot continue") {
    for (int id = 40; id <= 53; ++id) {
      CAPTURE(id);
      fixture.Prepare(true);
      fixture.Request().actionId = id;
      CHECK(Battle_CanTriggerBattleAbility(0) == 0);
    }
  }
}

TEST_CASE("Ability timing honors exact difficulty and chain-depth boundaries",
          "[combat][abilities][timing]") {
  // BATTLE.PRG func_80078AB4, 146C.c:7147.
  const int role = GENERATE(0, 1, 2);
  const bool defense = role != 0;
  const bool spell = role == 2;
  const int difficulty = GENERATE(0, 1);
  const unsigned depth = GENERATE(0u, 3u, 24u, 99u);
  const int slot = GENERATE(1, 2, 3);
  const int margin = std::max(
      2, (difficulty ? 8 : 10) + (spell ? 4 : 0) - static_cast<int>(depth / 3));
  for (int delta : {-margin, -margin + 1, 0, 1, margin, margin + 1, 2 * margin,
                    2 * margin + 1}) {
    CAPTURE(defense, spell, difficulty, depth, slot, margin, delta);
    AbilityFixture fixture;
    fixture.Prepare(defense, spell, depth);
    constexpr std::array kButtons{PADRright, PADRup, PADRleft};
    Input_buttonsPressed.allButtons = kButtons[slot - 1];
    State_SetScriptFlag(1, static_cast<char>(difficulty));
    fixture.action.abilityTimer = 100 + delta;
    const auto streak = Audio_Seq_unusedScoredata.chainStreak;
    Battle_UpdateBattleAbilityTiming();
    const bool success = delta > 0 && delta <= margin;
    const int result = delta <= -margin      ? 0
                       : delta <= 0          ? 1
                       : delta <= margin     ? 2
                       : delta <= 2 * margin ? 3
                                             : 4;
    REQUIRE(Battle_floatingLabelCount == 1);
    CHECK(fixture.labels[0].value == result);
    CHECK(fixture.action.abilityInputAttempted == 1);
    fixture.CheckInputResult(success ? (defense ? 2 : 1) : 0,
                             success ? slot : 0);
    CHECK(fixture.action.abilityTimer == 102 + delta);
    CHECK(Audio_Seq_unusedScoredata.chainStreak ==
          streak + (defense && success && streak < 65535 ? 1 : 0));
  }
}

TEST_CASE("Unarmed windows and repeated buttons cannot start an ability",
          "[combat][abilities][timing]") {
  AbilityFixture fixture;
  fixture.action.abilityTimer = 102;
  SECTION("input before a timing window") {
    fixture.action.abilityWindow = 0;
    Battle_UpdateBattleAbilityTiming();
    CHECK(fixture.action.abilityInputAttempted == 1);
    REQUIRE(Battle_floatingLabelCount == 1);
    CHECK(fixture.labels[0].value == 1);
  }
  SECTION("repeat previous chain button") {
    fixture.action.previousChainInput = 1;
    Battle_UpdateBattleAbilityTiming();
    CHECK(fixture.action.abilityInputAttempted == 1);
    CHECK(Battle_floatingLabelCount == 0);
  }
  CHECK(fixture.action.abilityInputSucceeded == 0);
  CHECK(fixture.action.chainAbilitySlot == 0);
}

TEST_CASE("Unmapped buttons and no input cannot start an ability",
          "[combat][abilities][timing]") {
  AbilityFixture fixture;
  fixture.action.abilityTimer = 102;
  SECTION("unmapped button") {
    Input_settings.mappedChainAbilities[0] = 0;
    Battle_UpdateBattleAbilityTiming();
    CHECK(fixture.action.abilityInputAttempted == 0);
    CHECK(Battle_floatingLabelCount == 0);
  }
  SECTION("no button") {
    Input_buttonsPressed.allButtons = 0;
    Battle_UpdateBattleAbilityTiming();
    CHECK(fixture.action.abilityInputAttempted == 0);
    CHECK(Battle_floatingLabelCount == 0);
  }
  CHECK(fixture.action.abilityInputSucceeded == 0);
  CHECK(fixture.action.chainAbilitySlot == 0);
}

TEST_CASE("Simultaneous ability buttons keep the original input priority",
          "[combat][abilities][timing]") {
  const auto [buttons, slot] =
      GENERATE(std::make_pair(PADRright | PADRup, 2),
               std::make_pair(PADRright | PADRleft, 3),
               std::make_pair(PADRup | PADRleft, 3),
               std::make_pair(PADRright | PADRup | PADRleft, 3));
  AbilityFixture fixture;
  fixture.action.abilityTimer = 102;
  Input_buttonsPressed.allButtons = buttons;
  Battle_UpdateBattleAbilityTiming();
  fixture.CheckInputResult(1, slot);
}

TEST_CASE("Successful defensive timing saturates rather than wraps its score",
          "[combat][abilities][timing]") {
  const auto [before, after] =
      GENERATE(std::make_pair(0, 1), std::make_pair(65534, 65535),
               std::make_pair(65535, 65535));
  AbilityFixture fixture;
  fixture.Prepare(true);
  fixture.action.abilityTimer = 102;
  Audio_Seq_unusedScoredata.chainStreak = before;
  Battle_UpdateBattleAbilityTiming();
  fixture.CheckInputResult(2, 1);
  CHECK(Audio_Seq_unusedScoredata.chainStreak == after);
}
