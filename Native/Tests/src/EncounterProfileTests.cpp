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
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Action.hpp"
#include "Battle/ActionMenu.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Encounter.hpp"
#include "CombatLab/Trace.hpp"
#include "Content/DataAccess.hpp"
#include "Content/DataStore.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/EncounterSpawnProfile.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Setup/Assembly.hpp"
#include "Menu/Setup/SetupLive.hpp"
#include "State/Progress.hpp"
#include "State/StateFlags.hpp"
#include "Title/NewGame.hpp"

using VS::Battle::kActionTypeBreakArt;
using VS::Battle::kActionTypeItem;

namespace {
struct ProfileFixture {
  decltype(VS::Input::settings) saved_settings = VS::Input::settings;
  decltype(VS::State::stateFlags) saved_flags = VS::State::stateFlags;
  decltype(VS::State::artsStatus) saved_arts = VS::State::artsStatus;
  decltype(VS::Items::inventory) saved_inventory = VS::Items::inventory;
  decltype(VS::Items::inventoryIndices) saved_indices =
      VS::Items::inventoryIndices;
  decltype(std::to_array(VS::State::learnedActionFlags)) saved_learned =
      std::to_array(VS::State::learnedActionFlags);
  decltype(VS::Battle::actionDefinitions) saved_actions =
      VS::Battle::actionDefinitions;
  decltype(VS::Battle::chainAbilityActionIds) saved_chain =
      VS::Battle::chainAbilityActionIds;
  decltype(VS::Battle::defenseAbilityActionIds) saved_defense =
      VS::Battle::defenseAbilityActionIds;
  decltype(VS::Battle::spellClassCounts) saved_counts =
      VS::Battle::spellClassCounts;
  decltype(VS::Battle::spellClassSpellIds) saved_spells =
      VS::Battle::spellClassSpellIds;
  std::array<std::uint8_t, 4> spells{97, 54, 60, 84};

  ProfileFixture() {
    VS::Input::settings = {};
    VS::Input::settings.menu_flags = 0x2D8;
    VS::State::stateFlags.intro_state = 0;
    VS::Items::inventory = {};
    VS::Items::inventory.weapons[0].is_equipped = 1;
    VS::Items::inventory.weapons[0].blade = 1;
    VS::Items::inventory.blades[0].category = 3;
    std::fill_n(VS::State::learnedActionFlags, saved_learned.size(), 0);
    const std::array<char, 3> chain{22, 23, 24}, defense{40, 41, 42};
    std::copy(chain.begin(), chain.end(),
              VS::Battle::chainAbilityActionIds.begin());
    std::copy(defense.begin(), defense.end(),
              VS::Battle::defenseAbilityActionIds.begin());
    for (std::size_t index = 0; index < spells.size(); ++index) {
      VS::Battle::spellClassCounts[index] = 1;
      VS::Battle::spellClassSpellIds[index] = &spells[index];
    }
  }

  ~ProfileFixture() {
    VS::Input::settings = saved_settings;
    VS::State::stateFlags = saved_flags;
    VS::State::artsStatus = saved_arts;
    VS::Items::inventory = saved_inventory;
    VS::Items::inventoryIndices = saved_indices;
    std::copy(saved_learned.begin(), saved_learned.end(),
              VS::State::learnedActionFlags);
    std::copy(saved_actions.begin(), saved_actions.end(),
              VS::Battle::actionDefinitions.data());
    std::copy(saved_chain.begin(), saved_chain.end(),
              VS::Battle::chainAbilityActionIds.begin());
    std::copy(saved_defense.begin(), saved_defense.end(),
              VS::Battle::defenseAbilityActionIds.begin());
    std::copy(saved_counts.begin(), saved_counts.end(),
              VS::Battle::spellClassCounts.begin());
    std::copy(saved_spells.begin(), saved_spells.end(),
              VS::Battle::spellClassSpellIds.begin());
  }
};

struct StatFixture {
  VS::Battle::Actor* previous = VS::Battle::actors[0];
  VS::Battle::ActorStats stats{};
  VS::Battle::Actor actor{};

  StatFixture() {
    stats.current_hp = 42;
    stats.max_hp = 250;
    stats.current_mp = 17;
    stats.max_mp = 50;
    stats.strength = stats.total_strength = 100;
    stats.intelligence = stats.total_intelligence = 101;
    stats.agility = stats.total_agility = 102;
    stats.risk = 7;
    actor.stats = &stats;
    VS::Battle::actors[0] = &actor;
  }
  ~StatFixture() { VS::Battle::actors[0] = previous; }
};

void CheckBaseAttributes(const VS::Battle::ActorStats& stats) {
  CHECK(stats.strength == 120);
  CHECK(stats.total_strength == 120);
  CHECK(stats.intelligence == 130);
  CHECK(stats.total_intelligence == 130);
  CHECK(stats.agility == 140);
  CHECK(stats.total_agility == 140);
}
void CheckStockedSlots(std::set<int>& supplied) {
  for (std::size_t slot = 0; slot < 58; ++slot) {
    const auto& item = VS::Items::inventory.misc[slot];
    CAPTURE(slot, item.id);
    CHECK(item.count == 10);
    CHECK(item.index == static_cast<int>(slot + 1));
    CHECK(VS::Items::inventoryIndices.misc[slot] == slot + 1);
    CHECK(supplied.insert(item.id).second);
  }
}

void CheckUnusedSlots() {
  for (std::size_t slot = 58; slot < 64; ++slot) {
    const auto& item = VS::Items::inventory.misc[slot];
    CAPTURE(slot);
    CHECK(item.id == 0);
    CHECK(item.count == 0);
    CHECK(item.index == 0);
    CHECK(VS::Items::inventoryIndices.misc[slot] == 0);
  }
}
std::set<unsigned> BindFullSpellClasses(
    std::array<std::array<std::uint8_t, 18>, 4>& classes) {
  // Original BATTLE.PRG spell-class tables at 0x835A8/0x835AC.
  classes = {{
      {97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 110, 114, 118, 122, 126,
       130, 94, 95},
      {96, 87, 88, 90, 91, 77},
      {60, 54, 61, 55, 62, 56, 63, 57, 75, 76, 82, 83, 85, 79, 80, 92, 93, 58},
      {66, 67, 68, 69, 70, 71, 72, 73},
  }};
  const std::array<std::uint8_t, 4> counts{18, 6, 18, 8};
  std::set<unsigned> expected;
  for (std::size_t index = 0; index < classes.size(); ++index) {
    VS::Battle::spellClassCounts[index] = counts[index];
    VS::Battle::spellClassSpellIds[index] = classes[index].data();
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
  std::copy(chains.begin(), chains.end(),
            VS::Battle::chainAbilityActionIds.begin());
  std::copy(defenses.begin(), defenses.end(),
            VS::Battle::defenseAbilityActionIds.begin());
  std::set<unsigned> expected(chains.begin(), chains.begin() + 14);
  expected.insert(defenses.begin(), defenses.begin() + 14);
  return expected;
}

void BindBreakArts() {
  for (unsigned action = 184; action < 224; ++action) {
    VS::Battle::actionDefinitions[action].type = kActionTypeBreakArt;
  }
}
}  // namespace

TEST_CASE("Prologue Lab profiles preserve normal starting restrictions",
          "[combat][profile]") {
  ProfileFixture fixture;
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kPrologue);
  CHECK(VS::Input::settings.menu_flags == 0x2D8);
  CHECK(VS::State::stateFlags.intro_state == 0);
  CHECK(std::all_of(std::begin(VS::State::learnedActionFlags),
                    std::end(VS::State::learnedActionFlags),
                    [](char value) { return value == 0; }));
}

TEST_CASE("Post-prologue Lab profiles enable original Setup and Score gates",
          "[combat][profile]") {
  ProfileFixture fixture;
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kPostPrologue);
  CHECK(VS::Input::settings.menu_flags == 0x3F8);
  CHECK(VS::State::stateFlags.intro_state == 3);
  CHECK(VS::Input::settings.mapped_chain_abilities[0] == 0);
}

TEST_CASE("Post-Minotaur Lab profiles grant only representative native actions",
          "[combat][profile]") {
  ProfileFixture fixture;
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kPostMinotaur);
  CHECK(VS::Battle::actionDefinitions[22].unlocked == 1);
  CHECK(VS::Battle::actionDefinitions[42].unlocked == 1);
  CHECK(VS::Battle::actionDefinitions[84].unlocked == 1);
  CHECK(VS::Battle::actionDefinitions[97].unlocked == 1);
  CHECK(VS::Battle::actionDefinitions[192].unlocked == 1);
  CHECK(VS::Battle::actionDefinitions[193].unlocked == 0);
  CHECK(VS::Input::settings.mapped_chain_abilities[0] == 22);
  CHECK(VS::Input::settings.mapped_defense_abilities[2] == 42);
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
  std::array<std::array<std::uint8_t, 18>, 4> classes{};
  const auto expected = BindFullSpellClasses(classes);
  REQUIRE(expected.size() == 71);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllSpells);
  for (unsigned action = 0; action < std::size(VS::Battle::actionDefinitions);
       ++action) {
    CAPTURE(action);
    CHECK(VS::Battle::actionDefinitions[action].unlocked ==
          expected.contains(action));
  }
}

TEST_CASE("All-spells profile preserves inventory equipment and resources",
          "[combat][profile][spells]") {
  ProfileFixture fixture;
  StatFixture actor;
  const auto inventory = VS::Items::inventory;
  const auto indices = VS::Items::inventoryIndices;
  const auto stats = actor.stats;
  std::array<std::array<std::uint8_t, 18>, 4> classes{};
  BindFullSpellClasses(classes);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllSpells);
  CHECK(std::memcmp(&inventory, &VS::Items::inventory, sizeof inventory) == 0);
  CHECK(std::memcmp(&indices, &VS::Items::inventoryIndices, sizeof indices) ==
        0);
  CHECK(std::memcmp(&stats, &actor.stats, sizeof stats) == 0);
  CHECK(VS::Input::settings.mapped_chain_abilities[0] == 0);
  CHECK(VS::Input::settings.mapped_defense_abilities[0] == 0);
  CHECK(VS::Input::settings.menu_flags == 0x3F8);
  CHECK(VS::State::stateFlags.intro_state == 3);
}

TEST_CASE("All-spells profile supports repeat entry and saved presets",
          "[combat][profile][spells]") {
  ProfileFixture fixture;
  std::array<std::array<std::uint8_t, 18>, 4> classes{};
  BindFullSpellClasses(classes);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllSpells);
  const auto learned = std::to_array(VS::State::learnedActionFlags);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllSpells);
  CHECK(std::to_array(VS::State::learnedActionFlags) == learned);
  VS::Game::EncounterSetup setup;
  setup.profile = VS::Game::EncounterProfile::kAllSpells;
  const auto json = VS::combat_lab::EncodeSetup(setup);
  CHECK(json.at("ashley_profile") == "all-spells");
  CHECK(VS::combat_lab::DecodeSetup(json).profile == setup.profile);
}

TEST_CASE("All-spells profile rejects missing production spell tables",
          "[combat][profile][spells]") {
  ProfileFixture fixture;
  SECTION("empty class") { VS::Battle::spellClassCounts[0] = 0; }
  SECTION("unbound class") { VS::Battle::spellClassSpellIds[0] = nullptr; }
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
  for (unsigned action = 0; action < std::size(VS::Battle::actionDefinitions);
       ++action) {
    CAPTURE(action);
    CHECK(VS::Battle::actionDefinitions[action].unlocked ==
          expected.contains(action));
  }
  for (unsigned slot = 0; slot < 3; ++slot) {
    CHECK(VS::Input::settings.mapped_chain_abilities[slot] ==
          static_cast<std::uint8_t>(VS::Battle::chainAbilityActionIds[slot]));
    CHECK(VS::Input::settings.mapped_defense_abilities[slot] ==
          static_cast<std::uint8_t>(VS::Battle::defenseAbilityActionIds[slot]));
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
  VS::State::stateFlags.teleportation_state = 1;
  VS::State::stateFlags.magic_used = 1;
  VS::State::artsStatus.arts_learned[0] = 2;
  VS::State::artsStatus.kills.total_kills = 321;
  VS::State::artsStatus.kills.weapon_category_kills[0] = 7;
  VS::State::artsStatus.kills.battle_abilities_unlocked = 4;
  auto expected_flags = VS::State::stateFlags;
  expected_flags.intro_state = 3;
  const auto arts = VS::State::artsStatus;
  const auto inventory = VS::Items::inventory;
  const auto indices = VS::Items::inventoryIndices;
  const auto stats = actor.stats;
  VS::Game::InitializeEncounterPlayerProfile(profile);
  CHECK(std::memcmp(&expected_flags, &VS::State::stateFlags,
                    sizeof expected_flags) == 0);
  CHECK(std::memcmp(&arts, &VS::State::artsStatus, sizeof arts) == 0);
  CHECK(std::memcmp(&inventory, &VS::Items::inventory, sizeof inventory) == 0);
  CHECK(std::memcmp(&indices, &VS::Items::inventoryIndices, sizeof indices) ==
        0);
  CHECK(std::memcmp(&stats, &actor.stats, sizeof stats) == 0);
  CHECK(VS::Input::settings.menu_flags == 0x3F8);
}

TEST_CASE("All-abilities profile supports repeat entry and saved presets",
          "[combat][profile][abilities]") {
  ProfileFixture fixture;
  BindFullBattleAbilities();
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllAbilities);
  const auto learned = std::to_array(VS::State::learnedActionFlags);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllAbilities);
  CHECK(std::to_array(VS::State::learnedActionFlags) == learned);
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
  SECTION("chain table") { VS::Battle::chainAbilityActionIds[0] = 0; }
  SECTION("defense table") { VS::Battle::defenseAbilityActionIds[0] = 0; }
  CHECK_THROWS(VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllAbilities));
}

TEST_CASE("All-Break-Arts profile grants all forty original arts only",
          "[combat][profile][break-arts]") {
  ProfileFixture fixture;
  BindBreakArts();
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllBreakArts);
  for (unsigned action = 0; action < std::size(VS::Battle::actionDefinitions);
       ++action) {
    CAPTURE(action);
    CHECK(VS::Battle::actionDefinitions[action].unlocked ==
          (action >= 184 && action < 224));
  }
  const auto learned = std::to_array(VS::State::learnedActionFlags);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllBreakArts);
  CHECK(std::to_array(VS::State::learnedActionFlags) == learned);
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
  VS::Battle::actionDefinitions[223].type = kActionTypeItem;
  CHECK_THROWS(VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllBreakArts));
}

TEST_CASE("All-Break-Arts profile preserves existing ability mappings",
          "[combat][profile][break-arts]") {
  ProfileFixture fixture;
  BindBreakArts();
  VS::Input::settings.mapped_chain_abilities[0] = 22;
  VS::Input::settings.mapped_defense_abilities[0] = 40;
  const auto chain = std::to_array(VS::Input::settings.mapped_chain_abilities);
  const auto defense =
      std::to_array(VS::Input::settings.mapped_defense_abilities);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kAllBreakArts);
  CHECK(std::to_array(VS::Input::settings.mapped_chain_abilities) == chain);
  CHECK(std::to_array(VS::Input::settings.mapped_defense_abilities) == defense);
}

TEST_CASE(
    "Two item profiles cover named misc items without expanding inventory",
    "[combat][profile][items]") {
  ProfileFixture fixture;
  const auto equipment = VS::Items::inventory;
  const auto learned = std::to_array(VS::State::learnedActionFlags);
  std::set<int> supplied;
  for (const auto profile : {VS::Game::EncounterProfile::kAllItemsA,
                             VS::Game::EncounterProfile::kAllItemsB}) {
    VS::Game::InitializeEncounterPlayerProfile(profile);
    CHECK(std::size(VS::Items::inventory.misc) == 64);
    CHECK(std::memcmp(&equipment, &VS::Items::inventory,
                      offsetof(VS::Items::InventoryContents, misc)) == 0);
    CHECK(std::to_array(VS::State::learnedActionFlags) == learned);
    CHECK(VS::Input::settings.menu_flags == 0x3F8);
    CHECK(VS::State::stateFlags.intro_state == 3);
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
    const auto stock = VS::Items::inventory;
    const auto indices = VS::Items::inventoryIndices;
    VS::Items::inventory.misc[0].count = 0;
    VS::Items::inventory.misc[63] = {323, 1, 64};
    VS::Items::inventoryIndices.misc[63] = 64;
    VS::Game::InitializeEncounterPlayerProfile(profile);
    CHECK(std::memcmp(&stock, &VS::Items::inventory, sizeof(stock)) == 0);
    CHECK(std::memcmp(&indices, &VS::Items::inventoryIndices,
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
  const auto inventory = VS::Items::inventory;
  CHECK_FALSE(
      VS::Game::AdjustEncounterPlayer(VS::Game::EncounterAdjustment::kLowerHp));
  CHECK_FALSE(VS::Game::AdjustEncounterPlayer(
      VS::Game::EncounterAdjustment::kRestoreMp));
  CHECK_FALSE(
      VS::Game::AdjustEncounterPlayer(VS::Game::EncounterAdjustment::kCount));
  CHECK(std::memcmp(&inventory, &VS::Items::inventory, sizeof(inventory)) == 0);
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
  const auto inventory = VS::Items::inventory;
  const auto learned = VS::Battle::actionDefinitions;
  const VS::Game::EncounterStatOverrides overrides{999, 200, 120, 130, 140};
  VS::Game::InitializeEncounterPlayerStats(overrides);
  CHECK(fixture.stats.current_hp == 999);
  CHECK(fixture.stats.max_hp == 999);
  CHECK(fixture.stats.current_mp == 200);
  CHECK(fixture.stats.max_mp == 200);
  CheckBaseAttributes(fixture.stats);
  CHECK(fixture.stats.risk == 7);
  CHECK(std::memcmp(&inventory, &VS::Items::inventory, sizeof(inventory)) == 0);
  CHECK(std::memcmp(learned.data(), VS::Battle::actionDefinitions.data(),
                    sizeof(VS::Battle::actionDefinitions)) == 0);
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
  CHECK(fixture.stats.current_hp == original.current_hp);
  CHECK(fixture.stats.max_hp == original.max_hp);
  CHECK(fixture.stats.total_strength == original.total_strength);
  CHECK(fixture.stats.current_mp == 999);
  CHECK(fixture.stats.max_mp == 999);
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
  VS::Battle::actors[0] = nullptr;
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
       {nlohmann::json::array(), nlohmann::json(223), nlohmann::json({130}),
        nlohmann::json({229}), nlohmann::json({236}), nlohmann::json({284}),
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
  auto expected = VS::Items::inventory;
  VS::Game::InitializeEncounterPlayerWeapon(Weapon::kDefault);
  CHECK(std::memcmp(&expected, &VS::Items::inventory, sizeof expected) == 0);
  for (auto& weapon : expected.weapons) weapon.is_equipped = 0;
  VS::Game::InitializeEncounterPlayerWeapon(Weapon::kBareHands);
  CHECK(std::memcmp(&expected, &VS::Items::inventory, sizeof expected) == 0);
  VS::Game::InitializeEncounterPlayerProfile(
      VS::Game::EncounterProfile::kPostMinotaur);
  CHECK(VS::Battle::actionDefinitions[220].unlocked == 1);
  CHECK(VS::Battle::actionDefinitions[221].unlocked == 0);
}

namespace {
void OpenWeaponSourceData() {
  for (auto dir = std::filesystem::current_path(); dir != dir.parent_path();
       dir = dir.parent_path()) {
    const auto root = dir / "Native" / "data";
    if (std::filesystem::exists(root / VS::Content::DataStore::ManifestName)) {
      REQUIRE(VS::Content::DataStore::Open(root));
      REQUIRE(VS::Content::LoadModuleData() == 0);
      return;
    }
  }
  SKIP("Extracted Native/data is unavailable");
}

struct WeaponHeap {
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 384> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
};

void CheckWeaponStats(const VS::Items::WeaponLoadout& actual,
                      VS::Items::WeaponSpawnLoadout source) {
  VS::Items::WeaponLoadout expected{};
  VS::Items::BuildLiveWeaponStatsFromParts(&expected, &source);
  CHECK(std::tie(actual.weapon_model_id, actual.blade.id, actual.grip.id,
                 actual.blade.material, actual.current_dp, actual.max_dp,
                 actual.current_pp, actual.max_pp,
                 actual.attributes.current_strength,
                 actual.attributes.current_intelligence,
                 actual.attributes.current_agility, actual.damage_type,
                 actual.damage_type_value, actual.risk,
                 actual.recovery_delay) ==
        std::tie(expected.weapon_model_id, expected.blade.id, expected.grip.id,
                 expected.blade.material, expected.current_dp, expected.max_dp,
                 expected.current_pp, expected.max_pp,
                 expected.attributes.current_strength,
                 expected.attributes.current_intelligence,
                 expected.attributes.current_agility, expected.damage_type,
                 expected.damage_type_value, expected.risk,
                 expected.recovery_delay));
  CHECK(std::memcmp(&actual.range, &expected.range, sizeof actual.range) == 0);
  CHECK(actual.name == expected.name);
  CHECK(std::memcmp(&actual.baseline, &expected.baseline,
                    sizeof actual.baseline) == 0);
}

VS::Items::InventoryContents CheckWeaponParts(
    const VS::Items::InventoryContents& original,
    VS::Items::InventoryWeapon& slot, int category) {
  auto& inventory = VS::Items::inventory;
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
  const auto index = static_cast<std::size_t>(&slot - inventory.weapons.data());
  auto& blade = inventory.blades[slot.blade - 1];
  auto& grip = inventory.grips[slot.grip - 1];
  CHECK(std::tuple(blade.id, grip.id, blade.weapon_model_id, blade.category) ==
        std::tuple(blades[category - 1], grips[category - 1],
                   models[category - 1], category));
  const auto typed_blade = blade;
  const auto typed_grip = grip;
  CHECK(VS::Menu::Setup::RunLiveSetup([&](VS::Menu::Setup::Context& context) {
          return VS::Menu::Setup::IsGripCompatible(context, &typed_blade,
                                                   &typed_grip)
                     ? 1
                     : 0;
        }) != 0);
  CHECK(std::tuple(blade.assembled_weapon_index, grip.assembled_weapon_index,
                   slot.blade, slot.grip) ==
        std::tuple(original.blades[slot.blade - 1].assembled_weapon_index,
                   original.grips[slot.grip - 1].assembled_weapon_index,
                   original.weapons[index].blade,
                   original.weapons[index].grip));
  auto expected = original;
  expected.blades[slot.blade - 1] = blade;
  expected.grips[slot.grip - 1] = grip;
  expected.weapons[index].name = slot.name;
  if (two_handed[category - 1]) {
    for (auto& shield : expected.shields) shield.is_equipped = 0;
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
  auto& inventory = VS::Items::inventory;
  std::memcpy(&inventory, VS::Title::kNewGameInventoryTemplate,
              sizeof inventory);
  if (GENERATE(false, true)) inventory.shields[0].is_equipped = 1;
  const auto original = inventory;
  const auto indices = VS::Items::inventoryIndices;
  for (int category = 1; category <= 9; ++category) {
    CAPTURE(category);
    const auto choice = static_cast<VS::Game::EncounterWeapon>(category);
    inventory = original;
    VS::Game::InitializeEncounterPlayerWeapon(choice);
    const auto slot = std::ranges::find_if(
        inventory.weapons,
        [](const auto& weapon) { return weapon.is_equipped != 0; });
    REQUIRE(slot != std::end(inventory.weapons));
    const auto expected = CheckWeaponParts(original, *slot, category);
    CHECK(std::memcmp(&inventory, &expected, sizeof inventory) == 0);
    CHECK(std::memcmp(&indices, &VS::Items::inventoryIndices, sizeof indices) ==
          0);
    const auto source = VS::Game::ReadEncounterWeapon(choice);
    REQUIRE(source);
    VS::Items::WeaponLoadout actual{};
    VS::Items::LoadLiveInventoryWeaponStats(&actual, &*slot);
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
  auto& inventory = VS::Items::inventory;
  inventory.weapons[0].grip = 1;
  SECTION("blade missing") { inventory.weapons[0].blade = 0; }
  SECTION("blade outside capacity") { inventory.weapons[0].blade = 17; }
  SECTION("grip missing") { inventory.weapons[0].grip = 0; }
  SECTION("grip outside capacity") { inventory.weapons[0].grip = 17; }
  SECTION("existing gems") { inventory.weapons[0].gems[0] = 1; }
  SECTION("no equipped weapon") { inventory.weapons[0].is_equipped = 0; }
  SECTION("multiple equipped weapons") { inventory.weapons[1].is_equipped = 1; }
  const auto expected = inventory;
  CHECK_THROWS(VS::Game::InitializeEncounterPlayerWeapon(
      VS::Game::EncounterWeapon::kDagger));
  CHECK(std::memcmp(&inventory, &expected, sizeof inventory) == 0);
}
