#include <algorithm>
#include <array>
#include <cstring>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Encounter.hpp"
#include "CombatLab/Trace.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Action.h"
#include "Battle/ActionMenu.h"
#include "Battle/ActionTable.h"
#include "Input/Controller.h"
#include "State/Progress.h"
}

namespace {
struct ProfileFixture {
  decltype(Input_settings) saved_settings = Input_settings;
  decltype(Audio_Seq_unusedStateFlags) saved_flags = Audio_Seq_unusedStateFlags;
  decltype(Audio_Seq_unusedInventory) saved_inventory =
      Audio_Seq_unusedInventory;
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
    Audio_Seq_unusedInventory = saved_inventory;
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
