#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>
#include <tuple>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Encounter.hpp"
#include "CombatLab/Trace.hpp"
#include "Content/DataAccess.h"
#include "Content/DataStore.hpp"
#include "Field/EncounterSpawnProfile.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Action.h"
#include "Battle/ActionMenu.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Core/Memory.h"
#include "Core/Memory/Heap.h"
#include "Input/Controller.h"
#include "Items/EquipmentStats.h"
#include "Menu/Setup/Assembly.h"
#include "State/Progress.h"
#include "Title/NewGame.h"
}

namespace {
struct ProfileFixture {
  decltype(Input_settings) saved_settings = Input_settings;
  decltype(Audio_Seq_unusedStateFlags) saved_flags = Audio_Seq_unusedStateFlags;
  decltype(Audio_Seq_unusedArtsStatus) saved_arts = Audio_Seq_unusedArtsStatus;
  decltype(Audio_Seq_unusedInventory) saved_inventory =
      Audio_Seq_unusedInventory;
  decltype(Audio_Seq_unusedInventoryIndices) saved_indices =
      Audio_Seq_unusedInventoryIndices;
  decltype(std::to_array(State_learnedActionFlags)) saved_learned =
      std::to_array(State_learnedActionFlags);
  decltype(std::to_array(Battle_actionDefinitions)) saved_actions =
      std::to_array(Battle_actionDefinitions);
  decltype(std::to_array(Battle_chainAbilityActionIds)) saved_chain =
      std::to_array(Battle_chainAbilityActionIds);
  decltype(std::to_array(Battle_defenseAbilityActionIds)) saved_defense =
      std::to_array(Battle_defenseAbilityActionIds);
  decltype(std::to_array(Battle_spellClassCounts)) saved_counts =
      std::to_array(Battle_spellClassCounts);
  decltype(std::to_array(Battle_spellClassSpellIds)) saved_spells =
      std::to_array(Battle_spellClassSpellIds);
  std::array<u_char, 4> spells{97, 54, 60, 84};

  ProfileFixture() {
    Input_settings = {};
    Input_settings.menuFlags = 0x2D8;
    Audio_Seq_unusedStateFlags.introState = 0;
    Audio_Seq_unusedInventory = {};
    Audio_Seq_unusedInventory.weapons[0].isEquipped = 1;
    Audio_Seq_unusedInventory.weapons[0].blade = 1;
    Audio_Seq_unusedInventory.blades[0].category = 3;
    std::fill_n(State_learnedActionFlags, saved_learned.size(), 0);
    const std::array<char, 3> chain{22, 23, 24}, defense{40, 41, 42};
    std::copy(chain.begin(), chain.end(), Battle_chainAbilityActionIds);
    std::copy(defense.begin(), defense.end(), Battle_defenseAbilityActionIds);
    for (std::size_t index = 0; index < spells.size(); ++index) {
      Battle_spellClassCounts[index] = 1;
      Battle_spellClassSpellIds[index] = &spells[index];
    }
  }

  ~ProfileFixture() {
    Input_settings = saved_settings;
    Audio_Seq_unusedStateFlags = saved_flags;
    Audio_Seq_unusedArtsStatus = saved_arts;
    Audio_Seq_unusedInventory = saved_inventory;
    Audio_Seq_unusedInventoryIndices = saved_indices;
    std::copy(saved_learned.begin(), saved_learned.end(),
              State_learnedActionFlags);
    std::copy(saved_actions.begin(), saved_actions.end(),
              Battle_actionDefinitions);
    std::copy(saved_chain.begin(), saved_chain.end(),
              Battle_chainAbilityActionIds);
    std::copy(saved_defense.begin(), saved_defense.end(),
              Battle_defenseAbilityActionIds);
    std::copy(saved_counts.begin(), saved_counts.end(),
              Battle_spellClassCounts);
    std::copy(saved_spells.begin(), saved_spells.end(),
              Battle_spellClassSpellIds);
  }
};

struct StatFixture {
  Battle_Actor* previous = Battle_actors[0];
  Battle_ActorStats stats{};
  Battle_Actor actor{};

  StatFixture() {
    stats.currentHp = 42;
    stats.maxHp = 250;
    stats.currentMp = 17;
    stats.maxMp = 50;
    stats.strength = stats.totalStrength = 100;
    stats.intelligence = stats.totalIntelligence = 101;
    stats.agility = stats.totalAgility = 102;
    stats.risk = 7;
    actor.stats = &stats;
    Battle_actors[0] = &actor;
  }
  ~StatFixture() { Battle_actors[0] = previous; }
};

void CheckBaseAttributes(const Battle_ActorStats& stats) {
  CHECK(stats.strength == 120);
  CHECK(stats.totalStrength == 120);
  CHECK(stats.intelligence == 130);
  CHECK(stats.totalIntelligence == 130);
  CHECK(stats.agility == 140);
  CHECK(stats.totalAgility == 140);
}
void CheckStockedSlots(std::set<int>& supplied) {
  for (std::size_t slot = 0; slot < 58; ++slot) {
    const auto& item = Audio_Seq_unusedInventory.misc[slot];
    CAPTURE(slot, item.id);
    CHECK(item.count == 10);
    CHECK(item.index == static_cast<int>(slot + 1));
    CHECK(Audio_Seq_unusedInventoryIndices.misc[slot] == slot + 1);
    CHECK(supplied.insert(item.id).second);
  }
}

void CheckUnusedSlots() {
  for (std::size_t slot = 58; slot < 64; ++slot) {
    const auto& item = Audio_Seq_unusedInventory.misc[slot];
    CAPTURE(slot);
    CHECK(item.id == 0);
    CHECK(item.count == 0);
    CHECK(item.index == 0);
    CHECK(Audio_Seq_unusedInventoryIndices.misc[slot] == 0);
  }
}
std::set<unsigned> BindFullSpellClasses(
    std::array<std::array<u_char, 18>, 4>& classes) {
  // Original BATTLE.PRG spell-class tables at 0x835A8/0x835AC.
  classes = {{
      {97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 110, 114, 118, 122, 126,
       130, 94, 95},
      {96, 87, 88, 90, 91, 77},
      {60, 54, 61, 55, 62, 56, 63, 57, 75, 76, 82, 83, 85, 79, 80, 92, 93, 58},
      {66, 67, 68, 69, 70, 71, 72, 73},
  }};
  const std::array<u_char, 4> counts{18, 6, 18, 8};
  std::set<unsigned> expected;
  for (std::size_t index = 0; index < classes.size(); ++index) {
    Battle_spellClassCounts[index] = counts[index];
    Battle_spellClassSpellIds[index] = classes[index].data();
    expected.insert(classes[index].begin(),
                    classes[index].begin() + counts[index]);
  }
  for (unsigned action = 106; action <= 133; ++action) expected.insert(action);
  return expected;
}

std::set<unsigned> BindFullBattleAbilities() {
  const std::array<char, 16> chains{22, 23, 24, 25, 26, 27, 28, 29,
                                    30, 31, 32, 34, 35, 37, 97, 98};
  const std::array<char, 16> defenses{40, 41, 42, 43, 44, 45, 46, 47,
                                      48, 49, 50, 51, 52, 53, 96, 60};
  std::copy(chains.begin(), chains.end(), Battle_chainAbilityActionIds);
  std::copy(defenses.begin(), defenses.end(), Battle_defenseAbilityActionIds);
  std::set<unsigned> expected(chains.begin(), chains.begin() + 14);
  expected.insert(defenses.begin(), defenses.begin() + 14);
  return expected;
}

void BindBreakArts() {
  for (unsigned action = 184; action < 224; ++action) {
    Battle_actionDefinitions[action].type = Battle_ActionType_BreakArt;
  }
}
}  // namespace

TEST_CASE("Prologue Lab profiles preserve normal starting restrictions",
          "[combat][profile]") {
  ProfileFixture fixture;
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kPrologue);
  CHECK(Input_settings.menuFlags == 0x2D8);
  CHECK(Audio_Seq_unusedStateFlags.introState == 0);
  CHECK(std::all_of(std::begin(State_learnedActionFlags),
                    std::end(State_learnedActionFlags),
                    [](char value) { return value == 0; }));
}

TEST_CASE("Post-prologue Lab profiles enable original Setup and Score gates",
          "[combat][profile]") {
  ProfileFixture fixture;
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kPostPrologue);
  CHECK(Input_settings.menuFlags == 0x3F8);
  CHECK(Audio_Seq_unusedStateFlags.introState == 3);
  CHECK(Input_settings.mappedChainAbilities[0] == 0);
}

TEST_CASE("Post-Minotaur Lab profiles grant only representative native actions",
          "[combat][profile]") {
  ProfileFixture fixture;
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kPostMinotaur);
  CHECK(Battle_actionDefinitions[22].unlocked == 1);
  CHECK(Battle_actionDefinitions[42].unlocked == 1);
  CHECK(Battle_actionDefinitions[84].unlocked == 1);
  CHECK(Battle_actionDefinitions[97].unlocked == 1);
  CHECK(Battle_actionDefinitions[192].unlocked == 1);
  CHECK(Battle_actionDefinitions[193].unlocked == 0);
  CHECK(Input_settings.mappedChainAbilities[0] == 22);
  CHECK(Input_settings.mappedDefenseAbilities[2] == 42);
}

TEST_CASE("Encounter presets persist Ashley profiles and accept older presets",
          "[combat][profile]") {
  VS::Game::EncounterSetup setup;
  setup.profile = VS::Game::EncounterProfile::kPostMinotaur;
  auto json = VS::combat_lab::EncodeSetup(setup);
  CHECK(json.at("ashley_profile") == "post-minotaur");
  CHECK(VS::combat_lab::DecodeSetup(json).profile == setup.profile);
  json.erase("ashley_profile");
  CHECK(VS::combat_lab::DecodeSetup(json).profile ==
        VS::Game::EncounterProfile::kPrologue);
  json["ashley_profile"] = "invalid";
  CHECK_THROWS(VS::combat_lab::DecodeSetup(json));
}

TEST_CASE("All-spells profile learns every battle spell and rank only",
          "[combat][profile][spells]") {
  ProfileFixture fixture;
  std::array<std::array<u_char, 18>, 4> classes{};
  const auto expected = BindFullSpellClasses(classes);
  REQUIRE(expected.size() == 71);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllSpells);
  for (unsigned action = 0; action < std::size(Battle_actionDefinitions);
       ++action) {
    CAPTURE(action);
    CHECK(Battle_actionDefinitions[action].unlocked ==
          expected.contains(action));
  }
}

TEST_CASE("All-spells profile preserves inventory equipment and resources",
          "[combat][profile][spells]") {
  ProfileFixture fixture;
  StatFixture actor;
  const auto inventory = Audio_Seq_unusedInventory;
  const auto indices = Audio_Seq_unusedInventoryIndices;
  const auto stats = actor.stats;
  std::array<std::array<u_char, 18>, 4> classes{};
  BindFullSpellClasses(classes);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllSpells);
  CHECK(std::memcmp(&inventory, &Audio_Seq_unusedInventory, sizeof inventory) ==
        0);
  CHECK(std::memcmp(&indices, &Audio_Seq_unusedInventoryIndices,
                    sizeof indices) == 0);
  CHECK(std::memcmp(&stats, &actor.stats, sizeof stats) == 0);
  CHECK(Input_settings.mappedChainAbilities[0] == 0);
  CHECK(Input_settings.mappedDefenseAbilities[0] == 0);
  CHECK(Input_settings.menuFlags == 0x3F8);
  CHECK(Audio_Seq_unusedStateFlags.introState == 3);
}

TEST_CASE("All-spells profile supports repeat entry and saved presets",
          "[combat][profile][spells]") {
  ProfileFixture fixture;
  std::array<std::array<u_char, 18>, 4> classes{};
  BindFullSpellClasses(classes);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllSpells);
  const auto learned = std::to_array(State_learnedActionFlags);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllSpells);
  CHECK(std::to_array(State_learnedActionFlags) == learned);
  VS::Game::EncounterSetup setup;
  setup.profile = VS::Game::EncounterProfile::kAllSpells;
  const auto json = VS::combat_lab::EncodeSetup(setup);
  CHECK(json.at("ashley_profile") == "all-spells");
  CHECK(VS::combat_lab::DecodeSetup(json).profile == setup.profile);
}

TEST_CASE("All-spells profile rejects missing production spell tables",
          "[combat][profile][spells]") {
  ProfileFixture fixture;
  SECTION("empty class") { Battle_spellClassCounts[0] = 0; }
  SECTION("unbound class") { Battle_spellClassSpellIds[0] = nullptr; }
  CHECK_THROWS(VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllSpells));
}

TEST_CASE("All-abilities profile grants the original 28 abilities only",
          "[combat][profile][abilities]") {
  ProfileFixture fixture;
  const auto expected = BindFullBattleAbilities();
  REQUIRE(expected.size() == 28);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllAbilities);
  for (unsigned action = 0; action < std::size(Battle_actionDefinitions);
       ++action) {
    CAPTURE(action);
    CHECK(Battle_actionDefinitions[action].unlocked ==
          expected.contains(action));
  }
  for (unsigned slot = 0; slot < 3; ++slot) {
    CHECK(Input_settings.mappedChainAbilities[slot] ==
          Battle_chainAbilityActionIds[slot]);
    CHECK(Input_settings.mappedDefenseAbilities[slot] ==
          Battle_defenseAbilityActionIds[slot]);
  }
}

TEST_CASE("Ability profiles preserve resources and story progression",
          "[combat][profile][abilities][break-arts]") {
  const auto profile = GENERATE(VS::Game::EncounterProfile::kAllAbilities,
                                VS::Game::EncounterProfile::kAllBreakArts);
  ProfileFixture fixture;
  StatFixture actor;
  BindFullBattleAbilities();
  BindBreakArts();
  Audio_Seq_unusedStateFlags.opaqueB5 = 1;
  Audio_Seq_unusedStateFlags.magicUsed = 1;
  Audio_Seq_unusedArtsStatus.artsLearned[0] = 2;
  Audio_Seq_unusedArtsStatus.kills.totalKills = 321;
  Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[0] = 7;
  Audio_Seq_unusedArtsStatus.kills.battleAbilitiesUnlocked = 4;
  auto expected_flags = Audio_Seq_unusedStateFlags;
  expected_flags.introState = 3;
  const auto arts = Audio_Seq_unusedArtsStatus;
  const auto inventory = Audio_Seq_unusedInventory;
  const auto indices = Audio_Seq_unusedInventoryIndices;
  const auto stats = actor.stats;
  VS::Game::InitializeEncounterPlayerProfile(profile);
  CHECK(std::memcmp(&expected_flags, &Audio_Seq_unusedStateFlags,
                    sizeof expected_flags) == 0);
  CHECK(std::memcmp(&arts, &Audio_Seq_unusedArtsStatus, sizeof arts) == 0);
  CHECK(std::memcmp(&inventory, &Audio_Seq_unusedInventory, sizeof inventory) ==
        0);
  CHECK(std::memcmp(&indices, &Audio_Seq_unusedInventoryIndices,
                    sizeof indices) == 0);
  CHECK(std::memcmp(&stats, &actor.stats, sizeof stats) == 0);
  CHECK(Input_settings.menuFlags == 0x3F8);
}

TEST_CASE("All-abilities profile supports repeat entry and saved presets",
          "[combat][profile][abilities]") {
  ProfileFixture fixture;
  BindFullBattleAbilities();
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllAbilities);
  const auto learned = std::to_array(State_learnedActionFlags);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllAbilities);
  CHECK(std::to_array(State_learnedActionFlags) == learned);
  VS::Game::EncounterSetup setup;
  setup.profile = VS::Game::EncounterProfile::kAllAbilities;
  const auto json = VS::combat_lab::EncodeSetup(setup);
  CHECK(json.at("ashley_profile") == "all-abilities");
  CHECK(VS::combat_lab::DecodeSetup(json).profile == setup.profile);
}

TEST_CASE("All-abilities profile rejects missing production ability tables",
          "[combat][profile][abilities]") {
  ProfileFixture fixture;
  BindFullBattleAbilities();
  SECTION("chain table") { Battle_chainAbilityActionIds[0] = 0; }
  SECTION("defense table") { Battle_defenseAbilityActionIds[0] = 0; }
  CHECK_THROWS(VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllAbilities));
}

TEST_CASE("All-Break-Arts profile grants all forty original arts only",
          "[combat][profile][break-arts]") {
  ProfileFixture fixture;
  BindBreakArts();
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllBreakArts);
  for (unsigned action = 0; action < std::size(Battle_actionDefinitions);
       ++action) {
    CAPTURE(action);
    CHECK(Battle_actionDefinitions[action].unlocked ==
          (action >= 184 && action < 224));
  }
  const auto learned = std::to_array(State_learnedActionFlags);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllBreakArts);
  CHECK(std::to_array(State_learnedActionFlags) == learned);
  VS::Game::EncounterSetup setup;
  setup.profile = VS::Game::EncounterProfile::kAllBreakArts;
  const auto json = VS::combat_lab::EncodeSetup(setup);
  CHECK(json.at("ashley_profile") == "all-break-arts");
  CHECK(VS::combat_lab::DecodeSetup(json).profile == setup.profile);
}

TEST_CASE("All-Break-Arts profile rejects an incomplete original action table",
          "[combat][profile][break-arts]") {
  ProfileFixture fixture;
  BindBreakArts();
  Battle_actionDefinitions[223].type = Battle_ActionType_Item;
  CHECK_THROWS(VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllBreakArts));
}

TEST_CASE("All-Break-Arts profile preserves existing ability mappings",
          "[combat][profile][break-arts]") {
  ProfileFixture fixture;
  BindBreakArts();
  Input_settings.mappedChainAbilities[0] = 22;
  Input_settings.mappedDefenseAbilities[0] = 40;
  const auto chain = std::to_array(Input_settings.mappedChainAbilities);
  const auto defense = std::to_array(Input_settings.mappedDefenseAbilities);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllBreakArts);
  CHECK(std::to_array(Input_settings.mappedChainAbilities) == chain);
  CHECK(std::to_array(Input_settings.mappedDefenseAbilities) == defense);
}

TEST_CASE(
    "Two item profiles cover named misc items without expanding inventory",
    "[combat][profile][items]") {
  ProfileFixture fixture;
  const auto equipment = Audio_Seq_unusedInventory;
  const auto learned = std::to_array(State_learnedActionFlags);
  std::set<int> supplied;
  for (const auto profile : {VS::Game::EncounterProfile::kAllItemsA,
                             VS::Game::EncounterProfile::kAllItemsB}) {
    VS::Game::InitializeEncounterPlayerProfile(profile);
    CHECK(std::size(Audio_Seq_unusedInventory.misc) == 64);
    CHECK(std::memcmp(&equipment, &Audio_Seq_unusedInventory,
                      offsetof(Items_Inventory, misc)) == 0);
    CHECK(std::to_array(State_learnedActionFlags) == learned);
    CHECK(Input_settings.menuFlags == 0x3F8);
    CHECK(Audio_Seq_unusedStateFlags.introState == 3);
    CAPTURE(profile);
    CheckStockedSlots(supplied);
    CheckUnusedSlots();
  }
  CHECK(supplied.size() == 116);
  for (int id = 1; id < 512; ++id) {
    const bool named_misc =
        (id >= 323 && id <= 356) || (id >= 386 && id <= 395) ||
        (id >= 399 && id <= 419 && (id - 399) % 4 == 0) ||
        (id >= 423 && id <= 456) || (id >= 458 && id <= 489);
    CAPTURE(id);
    CHECK(supplied.contains(id) == named_misc);
  }
}

TEST_CASE("Item profile initialization resets stacks and obsolete indices",
          "[combat][profile][items]") {
  ProfileFixture fixture;
  for (const auto profile : {VS::Game::EncounterProfile::kAllItemsA,
                             VS::Game::EncounterProfile::kAllItemsB}) {
    VS::Game::InitializeEncounterPlayerProfile(profile);
    const auto stock = Audio_Seq_unusedInventory;
    const auto indices = Audio_Seq_unusedInventoryIndices;
    Audio_Seq_unusedInventory.misc[0].count = 0;
    Audio_Seq_unusedInventory.misc[63] = {323, 1, 64};
    Audio_Seq_unusedInventoryIndices.misc[63] = 64;
    VS::Game::InitializeEncounterPlayerProfile(profile);
    CHECK(std::memcmp(&stock, &Audio_Seq_unusedInventory, sizeof(stock)) == 0);
    CHECK(std::memcmp(&indices, &Audio_Seq_unusedInventoryIndices,
                      sizeof(indices)) == 0);
    VS::Game::EncounterSetup setup;
    setup.profile = profile;
    CHECK(VS::combat_lab::DecodeSetup(VS::combat_lab::EncodeSetup(setup))
              .profile == profile);
  }
}

TEST_CASE("Lab precondition controls reject inactive sessions and bad commands",
          "[combat][profile][items]") {
  ProfileFixture fixture;
  const auto inventory = Audio_Seq_unusedInventory;
  CHECK_FALSE(
      VS::Game::AdjustEncounterPlayer(VS::Game::EncounterAdjustment::kLowerHp));
  CHECK_FALSE(VS::Game::AdjustEncounterPlayer(
      VS::Game::EncounterAdjustment::kRestoreMp));
  CHECK_FALSE(
      VS::Game::AdjustEncounterPlayer(VS::Game::EncounterAdjustment::kCount));
  CHECK(std::memcmp(&inventory, &Audio_Seq_unusedInventory,
                    sizeof(inventory)) == 0);
}

TEST_CASE("Lab movement ailment uses Numbness rather than the leg injury flag",
          "[combat][profile][status]") {
  CHECK(std::string_view(VS::Game::EncounterAdjustmentName(
            VS::Game::EncounterAdjustment::kNumbness)) == "NUMBNESS");
  CHECK(static_cast<int>(VS::Game::EncounterAdjustment::kRestoreMp) ==
        static_cast<int>(VS::Game::EncounterAdjustment::kEquipmentDown) + 1);
}

TEST_CASE("Ashley startup overrides fill resources and set base attributes",
          "[combat][profile][items][stats]") {
  StatFixture fixture;
  const auto inventory = Audio_Seq_unusedInventory;
  const auto learned = std::to_array(Battle_actionDefinitions);
  const VS::Game::EncounterStatOverrides overrides{999, 200, 120, 130, 140};
  VS::Game::InitializeEncounterPlayerStats(overrides);
  CHECK(fixture.stats.currentHp == 999);
  CHECK(fixture.stats.maxHp == 999);
  CHECK(fixture.stats.currentMp == 200);
  CHECK(fixture.stats.maxMp == 200);
  CheckBaseAttributes(fixture.stats);
  CHECK(fixture.stats.risk == 7);
  CHECK(std::memcmp(&inventory, &Audio_Seq_unusedInventory,
                    sizeof(inventory)) == 0);
  CHECK(std::memcmp(learned.data(), Battle_actionDefinitions,
                    sizeof(Battle_actionDefinitions)) == 0);
}

TEST_CASE("Ashley startup overrides preserve omitted stats",
          "[combat][profile][items][stats]") {
  StatFixture fixture;
  const auto original = fixture.stats;
  VS::Game::InitializeEncounterPlayerStats({});
  CHECK(std::memcmp(&original, &fixture.stats, sizeof(original)) == 0);
  VS::Game::EncounterStatOverrides overrides;
  overrides.max_mp = 999;
  VS::Game::InitializeEncounterPlayerStats(overrides);
  CHECK(fixture.stats.currentHp == original.currentHp);
  CHECK(fixture.stats.maxHp == original.maxHp);
  CHECK(fixture.stats.totalStrength == original.totalStrength);
  CHECK(fixture.stats.currentMp == 999);
  CHECK(fixture.stats.maxMp == 999);
}

TEST_CASE("Ashley startup overrides reject invalid data before mutation",
          "[combat][profile][items][stats]") {
  StatFixture fixture;
  VS::Game::EncounterStatOverrides overrides;
  overrides.max_mp = 999;
  overrides.max_hp = 0;
  const auto before_invalid = fixture.stats;
  CHECK_THROWS(VS::Game::InitializeEncounterPlayerStats(overrides));
  CHECK(std::memcmp(&before_invalid, &fixture.stats, sizeof(before_invalid)) ==
        0);
  Battle_actors[0] = nullptr;
  CHECK_NOTHROW(VS::Game::InitializeEncounterPlayerStats({}));
  overrides.max_hp = 1;
  CHECK_THROWS(VS::Game::InitializeEncounterPlayerStats(overrides));
}

TEST_CASE("Ashley stat presets retain defaults and reject invalid numbers",
          "[combat][profile][items][stats]") {
  VS::Game::EncounterSetup setup;
  setup.stat_overrides = {999, 200, 120, 130, 140};
  auto json = VS::combat_lab::EncodeSetup(setup);
  CHECK(VS::combat_lab::DecodeSetup(json).stat_overrides ==
        setup.stat_overrides);
  json.erase("ashley_stats");
  CHECK(VS::combat_lab::DecodeSetup(json).stat_overrides ==
        VS::Game::EncounterStatOverrides{});
  for (const auto& invalid :
       {nlohmann::json(0), nlohmann::json(-1), nlohmann::json(1000),
        nlohmann::json(1.5), nlohmann::json(true), nlohmann::json("999"),
        nlohmann::json(nullptr),
        nlohmann::json(std::numeric_limits<std::uint64_t>::max())}) {
    json["ashley_stats"] = {{"max_mp", invalid}};
    CHECK_THROWS(VS::combat_lab::DecodeSetup(json));
  }
  json["ashley_stats"] = {{"misspelled_stat", 100}};
  CHECK_THROWS(VS::combat_lab::DecodeSetup(json));
  json["ashley_stats"] = nlohmann::json::array();
  CHECK_THROWS(VS::combat_lab::DecodeSetup(json));
  for (const auto* field :
       {"max_hp", "max_mp", "strength", "intelligence", "agility"}) {
    for (const int value : {1, 999}) {
      json["ashley_stats"] = {{field, value}};
      CHECK(VS::combat_lab::EncodeSetup(VS::combat_lab::DecodeSetup(json))
                .at("ashley_stats") == json.at("ashley_stats"));
    }
  }
}

TEST_CASE("Lab story progress is optional, bounded and round-trips",
          "[combat][profile][story]") {
  VS::Game::EncounterSetup setup;
  auto json = VS::combat_lab::EncodeSetup(setup);
  CHECK_FALSE(json.contains("story_progress"));
  CHECK_FALSE(VS::combat_lab::DecodeSetup(json).story_progress.has_value());
  for (const int value : {0, 58, 255}) {
    json["story_progress"] = value;
    const auto decoded = VS::combat_lab::DecodeSetup(json);
    REQUIRE(decoded.story_progress == value);
    CHECK(VS::combat_lab::EncodeSetup(decoded).at("story_progress") == value);
  }
  for (const auto& invalid :
       {nlohmann::json(-1), nlohmann::json(256), nlohmann::json(58.5),
        nlohmann::json("58"), nlohmann::json(true), nlohmann::json(nullptr)}) {
    json["story_progress"] = invalid;
    CHECK_THROWS(VS::combat_lab::DecodeSetup(json));
  }
}

TEST_CASE("Lab clear count is optional, bounded and round-trips",
          "[combat][profile][story]") {
  VS::Game::EncounterSetup setup;
  auto json = VS::combat_lab::EncodeSetup(setup);
  CHECK_FALSE(json.contains("clear_count"));
  CHECK_FALSE(VS::combat_lab::DecodeSetup(json).clear_count.has_value());
  for (const int value : {1, 2, 99}) {
    json["clear_count"] = value;
    const auto decoded = VS::combat_lab::DecodeSetup(json);
    REQUIRE(decoded.clear_count == value);
    CHECK(VS::combat_lab::EncodeSetup(decoded).at("clear_count") == value);
  }
  for (const auto& invalid :
       {nlohmann::json(0), nlohmann::json(100), nlohmann::json(-1),
        nlohmann::json(1.5), nlohmann::json("1"), nlohmann::json(true),
        nlohmann::json(nullptr)}) {
    json["clear_count"] = invalid;
    CHECK_THROWS(VS::combat_lab::DecodeSetup(json));
  }
}

TEST_CASE("Lab crossed puzzles accept only time-trial unlock flags",
          "[combat][profile][story]") {
  VS::Game::EncounterSetup setup;
  auto json = VS::combat_lab::EncodeSetup(setup);
  CHECK_FALSE(json.contains("crossed_puzzles"));
  CHECK(VS::combat_lab::DecodeSetup(json).crossed_puzzles.empty());
  json["crossed_puzzles"] = {223, 221, 242};
  const auto decoded = VS::combat_lab::DecodeSetup(json);
  REQUIRE(decoded.crossed_puzzles == std::vector<int>{223, 221, 242});
  CHECK(VS::combat_lab::EncodeSetup(decoded).at("crossed_puzzles") ==
        json.at("crossed_puzzles"));
  for (const auto& invalid :
       {nlohmann::json::array(), nlohmann::json(223),
        nlohmann::json({130}), nlohmann::json({229}), nlohmann::json({236}),
        nlohmann::json({284}),
        nlohmann::json({223, 223}), nlohmann::json({"223"})}) {
    json["crossed_puzzles"] = invalid;
    CHECK_THROWS(VS::combat_lab::DecodeSetup(json));
  }
}

TEST_CASE(
    "Lab weapon presets round-trip all families and preserve old defaults",
    "[combat][profile][weapon]") {
  using Weapon = VS::Game::EncounterWeapon;
  VS::Game::EncounterSetup setup;
  for (int index = 0; index < static_cast<int>(Weapon::kCount); ++index) {
    setup.weapon = static_cast<Weapon>(index);
    const auto json = VS::combat_lab::EncodeSetup(setup);
    CHECK(VS::combat_lab::DecodeSetup(json).weapon == setup.weapon);
  }
  auto json = VS::combat_lab::EncodeSetup(setup);
  json.erase("ashley_weapon");
  CHECK(VS::combat_lab::DecodeSetup(json).weapon == Weapon::kDefault);
  for (const auto& invalid : {nlohmann::json("invalid"), nlohmann::json(2),
                              nlohmann::json(nullptr), nlohmann::json(true)}) {
    json["ashley_weapon"] = invalid;
    CHECK_THROWS(VS::combat_lab::DecodeSetup(json));
  }
  setup.weapon = Weapon::kCount;
  CHECK_THROWS(VS::combat_lab::EncodeSetup(setup));
  CHECK_THROWS(VS::Game::InitializeEncounterPlayerWeapon(setup.weapon));
  CHECK_FALSE(VS::Game::ReadEncounterWeapon(Weapon::kDefault));
  CHECK_FALSE(VS::Game::ReadEncounterWeapon(Weapon::kBareHands));
}

TEST_CASE("Default and bare-hands setup preserve stock parts and inventory",
          "[combat][profile][weapon]") {
  using Weapon = VS::Game::EncounterWeapon;
  ProfileFixture fixture;
  auto expected = Audio_Seq_unusedInventory;
  VS::Game::InitializeEncounterPlayerWeapon(Weapon::kDefault);
  CHECK(std::memcmp(&expected, &Audio_Seq_unusedInventory, sizeof expected) ==
        0);
  for (auto& weapon : expected.weapons) weapon.isEquipped = 0;
  VS::Game::InitializeEncounterPlayerWeapon(Weapon::kBareHands);
  CHECK(std::memcmp(&expected, &Audio_Seq_unusedInventory, sizeof expected) ==
        0);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kPostMinotaur);
  CHECK(Battle_actionDefinitions[220].unlocked == 1);
  CHECK(Battle_actionDefinitions[221].unlocked == 0);
}

namespace {
void OpenWeaponSourceData() {
  for (auto dir = std::filesystem::current_path(); dir != dir.parent_path();
       dir = dir.parent_path()) {
    const auto root = dir / "Native" / "data";
    if (std::filesystem::exists(root / VS::Content::DataStore::ManifestName)) {
      REQUIRE(VS::Content::DataStore::Open(root));
      REQUIRE(Content_LoadModuleData() == 0);
      return;
    }
  }
  SKIP("Extracted Native/data is unavailable");
}

struct WeaponHeap {
  Core_HeapBlockHeader saved_heap = Core_legacyHeapList;
  Core_HeapBlockHeader saved_secondary = Core_legacySecondaryHeapList;
  std::array<Core_HeapBlockHeader, 256> heap{};
  WeaponHeap() { Core_Memory_InitializeHeap(heap.data(), sizeof heap); }
  ~WeaponHeap() {
    Core_legacyHeapList = saved_heap;
    Core_legacySecondaryHeapList = saved_secondary;
  }
};

void CheckWeaponStats(const Items_WeaponLoadout& actual,
                      Items_WeaponSpawnLoadout source) {
  Items_WeaponLoadout expected{};
  Items_BuildWeaponStatsFromParts(&expected, &source);
  CHECK(std::tie(actual.wepId, actual.blade.id, actual.grip.id,
                 actual.blade.material, actual.currentDp, actual.maxDp,
                 actual.currentPp, actual.maxPp, actual.currentStr,
                 actual.currentInt, actual.currentAgility, actual.damageType,
                 actual.damageTypeValue, actual.risk, actual.recoveryDelay) ==
        std::tie(expected.wepId, expected.blade.id, expected.grip.id,
                 expected.blade.material, expected.currentDp, expected.maxDp,
                 expected.currentPp, expected.maxPp, expected.currentStr,
                 expected.currentInt, expected.currentAgility,
                 expected.damageType, expected.damageTypeValue, expected.risk,
                 expected.recoveryDelay));
  CHECK(std::memcmp(&actual.range, &expected.range, sizeof actual.range) == 0);
  CHECK(std::memcmp(actual.name, expected.name, sizeof actual.name) == 0);
  CHECK(std::memcmp(&actual.classAffinityBaseline,
                    &expected.classAffinityBaseline,
                    sizeof actual.classAffinityBaseline) == 0);
}

Items_Inventory CheckWeaponParts(const Items_Inventory& original,
                                 Items_InventoryWeapon& slot, int category) {
  auto& inventory = Audio_Seq_unusedInventory;
  if (slot.blade == 0 || slot.blade > std::size(inventory.blades) ||
      slot.grip == 0 || slot.grip > std::size(inventory.grips)) {
    throw std::runtime_error(
        "equipped weapon part indices exceed inventory capacity");
  }
  const std::array<int, 9> blades{1, 15, 25, 35, 51, 60, 70, 71, 83};
  const std::array<int, 9> grips{96, 96, 96, 105, 106, 111, 105, 115, 121};
  const std::array<int, 9> models{1, 15, 28, 35, 55, 60, 68, 71, 83};
  const std::array<bool, 9> two_handed{false, false, true, false, true,
                                       false, true,  true, true};
  const auto index =
      static_cast<std::size_t>(&slot - std::begin(inventory.weapons));
  auto& blade = inventory.blades[slot.blade - 1];
  auto& grip = inventory.grips[slot.grip - 1];
  CHECK(std::tuple(blade.id, grip.id, blade.wepId, blade.category) ==
        std::tuple(blades[category - 1], grips[category - 1],
                   models[category - 1], category));
  CHECK(Menu_Setup_IsGripCompatible(&blade, &grip) != 0);
  CHECK(std::tuple(blade.assembledWeaponIndex, grip.assembledWeaponIndex,
                   slot.blade, slot.grip) ==
        std::tuple(original.blades[slot.blade - 1].assembledWeaponIndex,
                   original.grips[slot.grip - 1].assembledWeaponIndex,
                   original.weapons[index].blade,
                   original.weapons[index].grip));
  auto expected = original;
  expected.blades[slot.blade - 1] = blade;
  expected.grips[slot.grip - 1] = grip;
  std::copy_n(slot.name, std::size(slot.name), expected.weapons[index].name);
  if (two_handed[category - 1]) {
    for (auto& shield : expected.shields) shield.isEquipped = 0;
  }
  return expected;
}
}  // namespace

TEST_CASE(
    "All nine Lab weapon families use original compatible inventory parts",
    "[combat][profile][weapon][disc]") {
  OpenWeaponSourceData();
  ProfileFixture fixture;
  WeaponHeap heap;
  auto& inventory = Audio_Seq_unusedInventory;
  std::memcpy(&inventory, Title_newGameInventoryTemplate, sizeof inventory);
  if (GENERATE(false, true)) inventory.shields[0].isEquipped = 1;
  const auto original = inventory;
  const auto indices = Audio_Seq_unusedInventoryIndices;
  for (int category = 1; category <= 9; ++category) {
    CAPTURE(category);
    const auto choice = static_cast<VS::Game::EncounterWeapon>(category);
    inventory = original;
    VS::Game::InitializeEncounterPlayerWeapon(choice);
    const auto slot = std::ranges::find_if(
        inventory.weapons,
        [](const auto& weapon) { return weapon.isEquipped != 0; });
    REQUIRE(slot != std::end(inventory.weapons));
    const auto expected = CheckWeaponParts(original, *slot, category);
    CHECK(std::memcmp(&inventory, &expected, sizeof inventory) == 0);
    CHECK(std::memcmp(&indices, &Audio_Seq_unusedInventoryIndices,
                      sizeof indices) == 0);
    const auto source = VS::Game::ReadEncounterWeapon(choice);
    REQUIRE(source);
    Items_WeaponLoadout actual{};
    Items_LoadInventoryWeaponStats(&actual, slot);
    CheckWeaponStats(actual, *source);
    VS::Game::InitializeEncounterPlayerWeapon(choice);
    CHECK(std::memcmp(&inventory, &expected, sizeof inventory) == 0);
    inventory = original;
    VS::Game::InitializeEncounterPlayerWeapon(
        VS::Game::EncounterWeapon::kDefault);
    CHECK(std::memcmp(&inventory, &original, sizeof inventory) == 0);
  }
}

TEST_CASE("Weapon setup rejects invalid stock links without mutating inventory",
          "[combat][profile][weapon][disc]") {
  OpenWeaponSourceData();
  ProfileFixture fixture;
  auto& inventory = Audio_Seq_unusedInventory;
  inventory.weapons[0].grip = 1;
  SECTION("blade missing") { inventory.weapons[0].blade = 0; }
  SECTION("blade outside capacity") { inventory.weapons[0].blade = 17; }
  SECTION("grip missing") { inventory.weapons[0].grip = 0; }
  SECTION("grip outside capacity") { inventory.weapons[0].grip = 17; }
  SECTION("existing gems") { inventory.weapons[0].gems[0] = 1; }
  SECTION("no equipped weapon") { inventory.weapons[0].isEquipped = 0; }
  SECTION("multiple equipped weapons") { inventory.weapons[1].isEquipped = 1; }
  const auto expected = inventory;
  CHECK_THROWS(VS::Game::InitializeEncounterPlayerWeapon(
      VS::Game::EncounterWeapon::kDagger));
  CHECK(std::memcmp(&inventory, &expected, sizeof inventory) == 0);
}
