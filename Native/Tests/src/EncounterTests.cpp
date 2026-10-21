#include <algorithm>
#include <cstring>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Encounter.hpp"
#include "Field/EncounterSpawnProfile.hpp"

extern "C" {
#include "Boot/PlayerSettings.h"
#include "Input/Controller.h"
}

namespace {
using VS::Game::EncounterSetup;
using VS::Game::EncounterZone;
using VS::Game::ValidateEncounter;

EncounterZone TestZone() {
  EncounterZone zone;
  zone.id = 9;
  zone.rooms.push_back({0, "test-room", 32, 32, {4, 2, 0, 0}, {}});
  zone.enemies.push_back({0, "primary", 100, 10, false, {}, {}, -1});
  zone.enemies.push_back({1, "secondary", 200, 20, true, {}, {}, -1});
  zone.enemies.push_back(
      {2, "unavailable", 0, 0, false, "asset missing", {}, -1});
  return zone;
}

EncounterSetup TestSetup(int count = 1, int enemy = 0) {
  EncounterSetup setup;
  setup.zone = 9;
  setup.room = 0;
  setup.player = {4, 2, 0, 0};
  for (int index = 0; index < count; ++index) {
    setup.enemies.push_back({enemy, {index + 1, 8, 0, 0}});
  }
  return setup;
}
}  // namespace

TEST_CASE("Encounter validation reserves real primary and auxiliary slots",
          "[combat][encounter]") {
  const auto zone = TestZone();
  CHECK(ValidateEncounter(TestSetup(8), zone).empty());
  CHECK_FALSE(ValidateEncounter(TestSetup(9), zone).empty());
  CHECK(ValidateEncounter(TestSetup(4, 1), zone).empty());
  CHECK_FALSE(ValidateEncounter(TestSetup(5, 1), zone).empty());
  auto mixed = TestSetup(5);
  mixed.enemies[0].enemy = 1;
  CHECK(ValidateEncounter(mixed, zone).empty());
}

TEST_CASE("Encounter validation rejects overlaps and invalid content",
          "[combat][encounter]") {
  auto zone = TestZone();
  auto setup = TestSetup();
  SECTION("same tile") { setup.enemies[0].position = setup.player; }
  SECTION("negative offset") { setup.enemies[0].position.x = -1; }
  SECTION("room boundary") { setup.player.z = 32; }
  SECTION("elevation encoding") { setup.player.elevation = 4; }
  SECTION("direction encoding") { setup.enemies[0].position.direction = 2; }
  SECTION("wrong zone") { setup.zone = 10; }
  SECTION("unknown room") { setup.room = 30; }
  SECTION("unknown enemy") { setup.enemies[0].enemy = 30; }
  SECTION("missing assets") { setup.enemies[0].enemy = 2; }
  SECTION("room unavailable") { zone.rooms[0].unavailable_reason = "bad data"; }
  SECTION("no enemies") { setup.enemies.clear(); }
  CHECK_FALSE(ValidateEncounter(setup, zone).empty());
}

TEST_CASE("Independent encounter definitions retain per-instance positions",
          "[combat][encounter]") {
  const auto zone = TestZone();
  const auto original = TestSetup(3);
  auto edited = original;
  edited.enemies[0].position.x = 20;
  CHECK(original.enemies[0].position.x == 1);
  CHECK(edited.enemies[1].position.x == 2);
  CHECK(ValidateEncounter(original, zone).empty());
  CHECK(ValidateEncounter(edited, zone).empty());
}

TEST_CASE("Encounter placement preserves source behavior and loot",
          "[combat][encounter]") {
  Field_MpdEnemySpawn source{};
  source.enemyId = 3;
  source.facing = 2;
  source.settings[0] = 0x10;
  source.settings[1] = 0x20;
  source.settings[2] = 0x3F;
  source.settings[5] = 0x1D;
  source.itemId = 10;
  source.item2Id = 20;
  source.item3Id = 30;
  source.itemCount = 1;
  source.item2Count = 2;
  source.item3Count = 3;
  source.variant = 7;
  source.reserved26 = 8;
  source.flagValue = 9;
  source.statValue = 10;
  source.slot = 3;
  source.isHidden = 1;
  source.flagTest = 2;
  source.statTest = 3;
  source.sceneTest = 1;
  const auto original = source;
  const auto prepared =
      VS::Game::PrepareEncounterSpawn(source, {3, {11, 12, 2, 1}}, 4);
  REQUIRE(prepared.has_value());
  auto expected = source;
  expected.index = 4;
  expected.tileX = 11;
  expected.tileZ = 12;
  expected.height = 2;
  expected.direction = 1;
  expected.slot = 0;
  expected.isHidden = 0;
  expected.flagTest = 0;
  expected.statTest = 0;
  expected.sceneTest = 0;
  CHECK(std::memcmp(&*prepared, &expected, sizeof(expected)) == 0);
  CHECK(std::memcmp(&source, &original, sizeof(source)) == 0);
}

TEST_CASE("Encounter profiles reject unsupported activation and encodings",
          "[combat][encounter]") {
  Field_MpdEnemySpawn source{};
  source.facing = 2;
  VS::Game::EncounterSpawn spawn{0, {1, 2, 0, 0}};
  std::size_t index = 0;
  SECTION("object profile") { source.kind = 1; }
  SECTION("neutral profile") { source.facing = 0; }
  SECTION("script activation") { source.actorStateByte = 1; }
  SECTION("wrong enemy") { source.enemyId = 1; }
  SECTION("negative coordinate") { spawn.position.x = -1; }
  SECTION("coordinate overflow") { spawn.position.z = 256; }
  SECTION("elevation overflow") { spawn.position.elevation = 4; }
  SECTION("direction overflow") { spawn.position.direction = 2; }
  SECTION("instance budget") { index = 8; }
  CHECK_FALSE(VS::Game::PrepareEncounterSpawn(source, spawn, index));
}

TEST_CASE("Direct entry and title share usable player menu defaults",
          "[combat][encounter]") {
  const auto saved = Input_settings;
  Boot_InitializePlayerSettings();
  CHECK(Input_settings.menuFlags == 0x2D8);
  CHECK(Input_settings.information == 1);
  CHECK(Input_settings.simpleMap == 3);
  CHECK(Input_settings.containerEmpty == 1);
  CHECK(Input_settings.abilityTimingDisplay == 1);
  CHECK(Input_settings.cursorMemory == 1);
  Input_settings.menuFlags = 0;
  Boot_InitializePlayerSettings();
  CHECK(Input_settings.menuFlags == 0x2D8);
  Input_settings = saved;
}
