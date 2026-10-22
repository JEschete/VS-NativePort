#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <set>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Encounter.hpp"
#include "CombatLab/Trace.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Action.h"
#include "Battle/ActionMenu.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Input/Controller.h"
#include "State/Progress.h"
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

TEST_CASE("All-abilities profile preserves resources and story progression",
          "[combat][profile][abilities]") {
  ProfileFixture fixture;
  StatFixture actor;
  BindFullBattleAbilities();
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
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllAbilities);
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
