#include <algorithm>
#include <array>
#include <iterator>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Abilities.h"
#include "Battle/ActionEffects.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/StatusEffects.h"
#include "Effects/EffectResources.h"
#include "Effects/PurgeTables.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/Ring.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Menu/BattleAbilities.h"
#include "State/Flags.h"
#include "State/Progress.h"
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

// SLUS_010.40 _artsPointsRequirements / _battleAbilitiesPointsRequirements,
// file offsets 0x405E4 / 0x4065C.
constexpr std::array<std::array<u_short, 4>, 10> kBreakArtThresholds{{
    {10, 65, 175, 340},
    {20, 90, 235, 425},
    {25, 110, 260, 485},
    {18, 80, 210, 420},
    {20, 100, 245, 465},
    {15, 90, 215, 410},
    {20, 95, 205, 385},
    {15, 95, 220, 405},
    {20, 95, 230, 430},
    {30, 105, 250, 460},
}};
constexpr std::array<u_short, 22> kAbilityThresholds{{
    20,  54,   72,   95,   125,  165,  216,  283,  369,  482,  628,
    818, 1066, 1387, 1805, 2349, 3055, 3973, 5167, 6719, 8737, 11359,
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
    Battle_actionDefinitions[1].type = Battle_ActionType_BaseAttack;
    Battle_actionDefinitions[54].type = Battle_ActionType_Spell;
    Battle_actionDefinitions[99].type = Battle_ActionType_Spell;
    Battle_actionDefinitions[141].type = Battle_ActionType_BreakArt;
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

  void Prepare(bool defense = false, int action_id = 1, unsigned index = 0) {
    action = {};
    action.queueIndex = index;
    auto& request = Request();
    request.actionId = action_id;
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

using AbilityEffect = int (*)(Battle_ActionDefinition*, Battle_HitResult*,
                              Battle_HitResult*, int, int);

struct AbilityHit {
  int calculator;
  int factor;
  AbilityEffect apply;
  bool self = false;
};

struct AbilityEffectFixture : AbilityFixture {
  decltype(std::to_array(Battle_statCalculators)) saved_calculators =
      std::to_array(Battle_statCalculators);
  decltype(std::to_array(Battle_limbArmorDpLoss)) saved_limb_damage =
      std::to_array(Battle_limbArmorDpLoss);
  decltype(std::to_array(Battle_statusEffectParams)) saved_status_params =
      std::to_array(Battle_statusEffectParams);
  std::array<Battle_Ai_ActorAiState*, 2> saved_ai{Battle_Ai_actorAiStates[0],
                                                  Battle_Ai_actorAiStates[1]};
  u_int saved_last_value = Battle_lastActionStatValue;
  int saved_critical = Battle_criticalHitOccurred;
  Battle_Ai_ActorAiState ai[2]{};
  Battle_HitResult source{};
  Battle_HitResult target{};

  AbilityEffectFixture() {
    action.queueIndex = 3;
    action.selection.actionId = 99;
    action.selection.primaryTarget.hpEffect = 1;
    action.selection.primaryTarget.target.hp = 101;
    action.selection.primaryTarget.target.mp = 9;
    action.selection.targets[0].target.hp = 83;
    action.selection.targets[0].target.mp = 18;
    Battle_actionDefinitions[99].type = Battle_ActionType_Spell;
    Battle_actionDefinitions[99].cost = 25;
    Battle_actionDefinitions[99].unlocked = 1;
    Battle_InitializeHitResult(&source);
    Battle_InitializeHitResult(&target);
    target.target.targetActor = 1;
    for (int index = 0; index < 2; ++index) {
      Battle_Ai_actorAiStates[index] = &ai[index];
      auto& actor = stats[index];
      actor.maxHp = 1000;
      actor.maxMp = 300;
      actor.currentHp = index == 0 ? 600 : 800;
      actor.currentMp = index == 0 ? 100 : 200;
      actor.walkSpeed = actor.baseWalkSpeed = 8;
      actor.runSpeed = actor.baseRunSpeed = 12;
      actor.weapon.blade.id = 1;
      actor.weapon.currentDp = 100;
      actor.weapon.maxDp = 1000;
      actor.weapon.currentPp = 70;
      actor.weapon.maxPp = 255;
      actor.shield.base.id = 1;
      actor.shield.currentPp = 90;
      actor.shield.maxPp = 255;
      for (int limb = 0; limb < 6; ++limb) {
        actor.limbs[limb].hp = 50;
        actor.limbs[limb].maxHp = 100;
        actor.dpSplash[limb][limb] = 100;
      }
    }
    BindCalculators();
    Battle_statusEffectParams[12] = {0x1000, 0, 0, 0x1000, 0};
    Battle_statusEffectParams[13] = {0x2000, 15, 0, 0x2000, 0};
    Battle_statusEffectParams[14] = {0x4000, 0, 2, 0x20000, 0};
    Battle_statusEffectParams[15] = {0x8000, 15, 0, 0x8800, 0x10};
  }

  ~AbilityEffectFixture() {
    std::copy(saved_calculators.begin(), saved_calculators.end(),
              Battle_statCalculators);
    std::copy(saved_limb_damage.begin(), saved_limb_damage.end(),
              Battle_limbArmorDpLoss);
    std::copy(saved_status_params.begin(), saved_status_params.end(),
              Battle_statusEffectParams);
    Battle_lastActionStatValue = saved_last_value;
    Battle_criticalHitOccurred = saved_critical;
    for (int index = 0; index < 2; ++index) {
      Battle_Ai_actorAiStates[index] = saved_ai[index];
    }
  }

  static void BindCalculators() {
    // BATTLE.PRG _statCalculators, original pointer table at file 0x7FB0C.
    Battle_statCalculators[1] = Battle_LastActionStatValue;
    Battle_statCalculators[4] = Battle_ScalePreviousTargetHit;
    Battle_statCalculators[5] = Battle_ScaleSourceMissingHp;
    Battle_statCalculators[6] = Battle_ScaleTargetCurrentHp;
    Battle_statCalculators[8] = Battle_WeaponPpWithChainOffset;
    Battle_statCalculators[9] = Battle_ScaleIncomingSpellDamage;
    Battle_statCalculators[10] = Battle_ScaleIncomingPhysicalDamage;
    Battle_statCalculators[11] = Battle_ScaleSourceMissingMp;
    Battle_statCalculators[18] = Battle_ScalePreviousTargetHit;
    Battle_statCalculators[19] = Battle_ScaleIncomingSpellEffect;
    Battle_statCalculators[20] = Battle_ScaleIncomingPhysicalDamage;
    Battle_statCalculators[21] = Battle_ScaleIncomingSpellCost;
    Battle_statCalculators[22] = Battle_ScaleIncomingDamage;
    Battle_statCalculators[23] = Battle_SourceShieldPp;
  }

  void Queue(int id, int hit, const AbilityHit& contract) {
    auto& definition = Battle_actionDefinitions[id];
    definition.category = id <= 31 ? 14 : 0;
    definition.hits[hit].statCalculatorId = contract.calculator;
    definition.hits[hit].statFactor = contract.factor;
    contract.apply(&definition, &source, contract.self ? &source : &target, hit,
                   0);
  }

  void Commit() {
    Battle_ApplyActorActionResult(&source);
    Battle_ApplyActorActionResult(&target);
  }

  void CheckResources(int source_hp, int source_mp, int target_hp,
                      int target_mp) const {
    CHECK(stats[0].currentHp == source_hp);
    CHECK(stats[0].currentMp == source_mp);
    CHECK(stats[1].currentHp == target_hp);
    CHECK(stats[1].currentMp == target_mp);
  }
};

struct AbilityUnlockFixture : AbilityFixture {
  State_ArtsStatus saved_progress = Audio_Seq_unusedArtsStatus;
  decltype(std::to_array(Battle_battleAbilityPointRequirements)) saved_points =
      std::to_array(Battle_battleAbilityPointRequirements);
  decltype(kBreakArtThresholds)::value_type saved_art_points[10]{};

  AbilityUnlockFixture() {
    Audio_Seq_unusedArtsStatus = {};
    std::fill_n(Audio_Seq_unusedArtsStatus.artsLearned, 12, 4);
    std::copy(kAbilityThresholds.begin(), kAbilityThresholds.end(),
              Battle_battleAbilityPointRequirements);
    for (unsigned category = 0; category < kBreakArtThresholds.size();
         ++category) {
      saved_art_points[category] =
          std::to_array(Battle_breakArtPointRequirements[category]);
      std::copy(kBreakArtThresholds[category].begin(),
                kBreakArtThresholds[category].end(),
                Battle_breakArtPointRequirements[category]);
    }
  }

  ~AbilityUnlockFixture() {
    Audio_Seq_unusedArtsStatus = saved_progress;
    std::copy(saved_points.begin(), saved_points.end(),
              Battle_battleAbilityPointRequirements);
    for (unsigned category = 0; category < kBreakArtThresholds.size();
         ++category) {
      std::copy(saved_art_points[category].begin(),
                saved_art_points[category].end(),
                Battle_breakArtPointRequirements[category]);
    }
  }
};

void CheckAbilityProgress(const State_ArtsStatus& before) {
  CHECK(std::to_array(Audio_Seq_unusedArtsStatus.artsLearned) ==
        std::to_array(before.artsLearned));
  CHECK(std::to_array(Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills) ==
        std::to_array(before.kills.weaponCategoryKills));
  CHECK(Audio_Seq_unusedArtsStatus.kills.battleAbilitiesUnlocked ==
        before.kills.battleAbilitiesUnlocked);
  CHECK(Audio_Seq_unusedArtsStatus.kills.totalKills == before.kills.totalKills);
}
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

TEST_CASE("Ability rewards use all original cumulative thresholds",
          "[combat][abilities][acquisition]") {
  AbilityUnlockFixture fixture;
  const int unlocked = GENERATE(Catch::Generators::range(0, 22));
  const int delta = GENERATE(-1, 0, 1);
  auto& progress = Audio_Seq_unusedArtsStatus;
  progress.kills.battleAbilitiesUnlocked = static_cast<u_short>(unlocked);
  progress.kills.totalKills =
      static_cast<u_short>(kAbilityThresholds[unlocked] + delta);
  const auto before = progress;
  CHECK(Battle_PendingBattleAbilityUnlocks() == (delta >= 0 ? 1 : 0));
  CheckAbilityProgress(before);
}

TEST_CASE("Pending Break Arts take priority over ability rewards",
          "[combat][abilities][acquisition]") {
  AbilityUnlockFixture fixture;
  const int category = GENERATE(Catch::Generators::range(0, 10));
  const int tier = GENERATE(Catch::Generators::range(0, 4));
  const int category_offset = GENERATE(0, 10);
  const int delta = GENERATE(-1, 0);
  const int unlocked = GENERATE(0, 21, 22);
  fixture.actors[0].equippedWeaponCategory =
      static_cast<u_char>(category + category_offset);
  auto& progress = Audio_Seq_unusedArtsStatus;
  progress.artsLearned[category] = static_cast<u_char>(tier);
  progress.kills.weaponCategoryKills[category] =
      static_cast<u_short>(kBreakArtThresholds[category][tier] + delta);
  progress.kills.totalKills = 65535;
  progress.kills.battleAbilitiesUnlocked = static_cast<u_short>(unlocked);
  const auto before = progress;
  CHECK(Battle_PendingBattleAbilityUnlocks() ==
        (delta == 0 ? 0x10000 : (unlocked < 22 ? 1 : 0)));
  CheckAbilityProgress(before);
}

TEST_CASE("Ability reward cap prevents further pending unlocks",
          "[combat][abilities][acquisition]") {
  AbilityUnlockFixture fixture;
  auto& progress = Audio_Seq_unusedArtsStatus;
  progress.kills.battleAbilitiesUnlocked =
      static_cast<u_short>(GENERATE(22, 23, 65535));
  progress.kills.totalKills = 65535;
  const auto before = progress;
  CHECK(Battle_PendingBattleAbilityUnlocks() == 0);
  CheckAbilityProgress(before);
}

TEST_CASE("Ineligible ability reward menus leave progression unchanged",
          "[combat][abilities][acquisition]") {
  AbilityUnlockFixture fixture;
  const int unlocked = GENERATE(Catch::Generators::range(0, 23));
  auto& progress = Audio_Seq_unusedArtsStatus;
  progress.kills.battleAbilitiesUnlocked = static_cast<u_short>(unlocked);
  progress.kills.totalKills = static_cast<u_short>(
      unlocked == 22 ? 65535 : kAbilityThresholds[unlocked] - 1);
  const auto before = progress;
  CHECK(Menu_BattleAbilities_UpdateAbilityUnlock(1) == 1);
  CheckAbilityProgress(before);
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
  const int role = GENERATE(0, 1, 2, 3, 4);
  const bool defense = role != 0;
  constexpr std::array kActions{1, 1, 54, 99, 141};
  const int action_id = kActions[role];
  const int difficulty = GENERATE(0, 1);
  const unsigned depth = GENERATE(0u, 3u, 24u, 99u);
  const int slot = GENERATE(1, 2, 3);
  const int margin =
      std::max(2, (difficulty ? 8 : 10) + (action_id >= 54 ? 4 : 0) -
                      static_cast<int>(depth / 3));
  for (int delta : {-margin, -margin + 1, 0, 1, margin, margin + 1, 2 * margin,
                    2 * margin + 1}) {
    CAPTURE(defense, action_id, difficulty, depth, slot, margin, delta);
    AbilityFixture fixture;
    fixture.Prepare(defense, action_id, depth);
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

TEST_CASE("Single-hit offensive abilities apply their original stat factors",
          "[combat][abilities][ability-effects]") {
  struct Example {
    int id;
    AbilityHit hit;
    int hp_change;
    int mp_change;
  };
  const auto examples = std::to_array<Example>({
      {22, {4, 7, Battle_QueueHitDamageAndEquipmentWear}, -72, 0},
      {23, {18, 3, Battle_AccumulateHpRestoration, true}, 32, 0},
      {24, {4, 3, Battle_AccumulateMpDamage}, 0, -32},
      {25, {18, 3, Battle_AccumulateMpRestoration, true}, 0, 32},
      {26, {5, 1, Battle_QueueHitDamageAndEquipmentWear}, -42, 0},
      {27, {11, 2, Battle_AccumulateMpDamage}, 0, -42},
  });
  for (const auto& example : examples) {
    CAPTURE(example.id);
    AbilityEffectFixture fixture;
    fixture.Queue(example.id, 0, example.hit);
    fixture.Commit();
    fixture.CheckResources(600 + (example.hit.self ? example.hp_change : 0),
                           100 + (example.hit.self ? example.mp_change : 0),
                           800 + (example.hit.self ? 0 : example.hp_change),
                           200 + (example.hit.self ? 0 : example.mp_change));
  }
}

TEST_CASE("Temper damages the target and restores weapon DP",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  fixture.Queue(28, 0, {4, 4, Battle_QueueHitDamageAndEquipmentWear});
  fixture.Queue(28, 1, {18, 20, Battle_AccumulateWeaponDpRestoration, true});
  fixture.Commit();
  fixture.CheckResources(600, 100, 758, 200);
  CHECK(fixture.stats[0].weapon.currentDp == 304);
}

TEST_CASE("Crimson Pain applies both enemy and self HP damage",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  fixture.Queue(29, 0, {6, 10, Battle_QueueHitDamageAndEquipmentWear});
  fixture.Queue(29, 1, {6, 3, Battle_QueueHitDamageAndEquipmentWear, true});
  fixture.Commit();
  fixture.CheckResources(580, 100, 718, 200);
}

TEST_CASE("Instill damages the target and restores weapon PP",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  fixture.Queue(30, 0, {4, 1, Battle_QueueHitDamageAndEquipmentWear});
  fixture.Queue(30, 1, {4, 10, Battle_AccumulateWeaponPpRestoration, true});
  fixture.Commit();
  fixture.CheckResources(600, 100, 788, 200);
  CHECK(fixture.stats[0].weapon.currentPp == 173);
}

TEST_CASE("Phantom Pain converts weapon PP to damage and empties it",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  fixture.Queue(31, 0, {8, 0, Battle_QueueHitDamageAndEquipmentWear});
  fixture.Queue(31, 1, {1, 0, Battle_AccumulateWeaponPpDrain, true});
  fixture.Commit();
  fixture.CheckResources(600, 100, 728, 200);
  CHECK(fixture.stats[0].weapon.currentPp == 0);
}

TEST_CASE("Siphon Soul restores half the incoming spell cost as MP",
          "[combat][abilities][ability-effects]") {
  const bool spell = GENERATE(false, true);
  AbilityEffectFixture fixture;
  fixture.action.selection.actionId = spell ? 99 : 1;
  fixture.Queue(41, 0, {21, 5, Battle_AccumulateMpRestoration, true});
  fixture.Commit();
  fixture.CheckResources(600, spell ? 112 : 100, 800, 200);
}

TEST_CASE("Reflect abilities return damage only for the matching action type",
          "[combat][abilities][ability-effects]") {
  const int id = GENERATE(42, 43);
  const bool spell = GENERATE(false, true);
  const int incoming_effect = GENERATE(0, 1, 2, 3);
  AbilityEffectFixture fixture;
  fixture.action.selection.actionId = spell ? 99 : 1;
  fixture.action.selection.primaryTarget.hpEffect = incoming_effect;
  fixture.Queue(id, 0,
                {id == 42 ? 9 : 10, 4, Battle_QueueHitDamageAndEquipmentWear});
  fixture.Commit();
  const bool applies =
      (spell == (id == 42)) && (incoming_effect == 1 || incoming_effect == 3);
  fixture.CheckResources(600, 100, applies ? 756 : 800, 200);
}

TEST_CASE("Absorb abilities recover damage only for the matching action type",
          "[combat][abilities][ability-effects]") {
  const int id = GENERATE(44, 45);
  const bool spell = GENERATE(false, true);
  const int incoming_effect = GENERATE(0, 1, 2, 3);
  AbilityEffectFixture fixture;
  fixture.action.selection.actionId = spell ? 99 : 1;
  fixture.action.selection.primaryTarget.hpEffect = incoming_effect;
  fixture.Queue(id, 0,
                {id == 44 ? 19 : 20, 2,
                 id == 44 ? Battle_QueueSpellTriggeredRecovery
                          : Battle_QueuePhysicalTriggeredRecovery,
                 true});
  fixture.Commit();
  const bool applies =
      (spell == (id == 44)) && (incoming_effect == 1 || incoming_effect == 3);
  fixture.CheckResources(applies ? 622 : 600, 100, 800, 200);
}

TEST_CASE("All seven affinity defenses recover only matching damage",
          "[combat][abilities][ability-effects]") {
  constexpr std::array<AbilityEffect, 7> kEffects{
      Battle_QueuePhysicalAffinityRecovery, Battle_QueueAirAffinityRecovery,
      Battle_QueueFireAffinityRecovery,     Battle_QueueEarthAffinityRecovery,
      Battle_QueueWaterAffinityRecovery,    Battle_QueueLightAffinityRecovery,
      Battle_QueueDarkAffinityRecovery};
  const int affinity = GENERATE(0, 1, 2, 3, 4, 5, 6);
  const int incoming = GENERATE(0, 1, 2, 3, 4, 5, 6);
  const int incoming_effect = GENERATE(1, 2, 3);
  AbilityEffectFixture fixture;
  fixture.action.selection.effectVariant = static_cast<char>(incoming);
  fixture.action.selection.primaryTarget.hpEffect = incoming_effect;
  fixture.Queue(46 + affinity, 0, {22, 5, kEffects[affinity], true});
  fixture.Commit();
  const bool applies = affinity == incoming && incoming_effect != 2;
  fixture.CheckResources(applies ? 655 : 600, 100, 800, 200);
}

TEST_CASE("Phantom Shield converts shield PP to recovery only after damage",
          "[combat][abilities][ability-effects]") {
  const int incoming_effect = GENERATE(0, 1, 2, 3);
  AbilityEffectFixture fixture;
  fixture.action.selection.primaryTarget.hpEffect = incoming_effect;
  fixture.Queue(53, 0, {23, 0, Battle_QueueDamageTriggeredRecovery, true});
  fixture.Queue(53, 1, {1, 0, Battle_AccumulateShieldPpDrain, true});
  fixture.Commit();
  const bool applies = incoming_effect == 1 || incoming_effect == 3;
  fixture.CheckResources(applies ? 690 : 600, 100, 800, 200);
  CHECK(fixture.stats[0].shield.currentPp == (applies ? 0 : 90));
}

TEST_CASE("Offensive status abilities queue and apply their original ailments",
          "[combat][abilities][ability-effects]") {
  struct Example {
    int id;
    int mask;
    AbilityEffect apply;
  };
  constexpr auto kExamples = std::to_array<Example>({
      {32, 0x2000, Battle_QueueParalysis},
      {34, 0x8000, Battle_QueueNumbness},
      {35, 0x1000, Battle_QueueSilence},
      {37, 0x4000, Battle_QueuePoison},
  });
  for (const auto& example : kExamples) {
    CAPTURE(example.id);
    AbilityEffectFixture fixture;
    fixture.Queue(example.id, 0, {0, 0, example.apply});
    CHECK(fixture.target.removeStatuses == example.mask);
    Battle_InflictActorStatuses(fixture.target.removeStatuses,
                                &fixture.stats[1]);
    CHECK(fixture.stats[1].statuses == example.mask);
    CHECK(fixture.stats[0].statuses == 0);
    fixture.CheckResources(600, 100, 800, 200);
  }
}

TEST_CASE("Ward removes paralysis and numbness without curing poison",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  Battle_InflictActorStatuses(0xE000, &fixture.stats[0]);
  fixture.Queue(40, 0, {0, 0, Battle_QueueParalysisCure, true});
  fixture.Queue(40, 1, {0, 0, Battle_QueueNumbnessCure, true});
  CHECK(fixture.source.inflictStatuses == 0xA000);
  Battle_RemoveActorStatuses(fixture.source.inflictStatuses, &fixture.stats[0]);
  CHECK(fixture.stats[0].statuses == 0x4000);
  CHECK(fixture.stats[0].walkSpeed == 8);
  CHECK(fixture.stats[0].runSpeed == 12);
  fixture.CheckResources(600, 100, 800, 200);
}

TEST_CASE("Restorative abilities respect HP MP and equipment capacity",
          "[combat][abilities][ability-effects]") {
  AbilityEffectFixture fixture;
  SECTION("Gain Life") {
    fixture.stats[0].currentHp = 995;
    fixture.Queue(23, 0, {18, 3, Battle_AccumulateHpRestoration, true});
    fixture.Commit();
    CHECK(fixture.stats[0].currentHp == 1000);
  }
  SECTION("Gain Magic") {
    fixture.stats[0].currentMp = 299;
    fixture.Queue(25, 0, {18, 3, Battle_AccumulateMpRestoration, true});
    fixture.Commit();
    CHECK(fixture.stats[0].currentMp == 300);
  }
  SECTION("Temper") {
    fixture.stats[0].weapon.currentDp = 995;
    fixture.Queue(28, 1, {18, 20, Battle_AccumulateWeaponDpRestoration, true});
    fixture.Commit();
    CHECK(fixture.stats[0].weapon.currentDp == 1000);
  }
  SECTION("Instill") {
    fixture.stats[0].weapon.currentPp = 250;
    fixture.Queue(30, 1, {4, 10, Battle_AccumulateWeaponPpRestoration, true});
    fixture.Commit();
    CHECK(fixture.stats[0].weapon.currentPp == 255);
  }
}

TEST_CASE("MP damage abilities cannot drain more MP than the target has",
          "[combat][abilities][ability-effects]") {
  const int id = GENERATE(24, 27);
  const int mp = GENERATE(0, 10, 200);
  AbilityEffectFixture fixture;
  fixture.stats[1].currentMp =
      static_cast<decltype(fixture.stats[1].currentMp)>(mp);
  fixture.Queue(
      id, 0, {id == 24 ? 4 : 11, id == 24 ? 3 : 2, Battle_AccumulateMpDamage});
  const int damage = std::min(mp, id == 24 ? 32 : 42);
  CHECK(fixture.target.target.mp == damage);
  CHECK(Battle_lastActionStatValue == damage);
  fixture.Commit();
  fixture.CheckResources(600, 100, 800, mp - damage);
}
