#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Encounter.hpp"
#include "Field/EncounterSpawnProfile.hpp"

extern "C" {
#include "Boot/PlayerSettings.h"
#include "Field/CollisionSampling.h"
#include "Input/Controller.h"
}

namespace {
using VS::Game::EncounterSetup;
using VS::Game::EncounterZone;
using VS::Game::ValidateEncounter;

EncounterZone TestZone() {
  EncounterZone zone;
  zone.id = 9;
  zone.rooms.push_back(
      {0,
       "test-room",
       32,
       32,
       {4, 2, 0, 0},
       {},
       std::vector<VS::Game::EncounterFloor>(
           std::size_t{32} * 32, VS::Game::EncounterFloor::kSupported)});
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
  SECTION("ordinary enemy") {}
  SECTION("boss retained for a story defeat event") {
    source.actorStateByte = 1;
  }
  SECTION("any nonzero retention byte") { source.actorStateByte = 255; }
  const auto original = source;
  CHECK(VS::Game::IsEncounterHostileSpawn(source));
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
  expected.actorStateByte = 0;
  CHECK(std::memcmp(&*prepared, &expected, sizeof(expected)) == 0);
  CHECK(std::memcmp(&source, &original, sizeof(source)) == 0);
}

TEST_CASE("Encounter profiles reject nonhostile records and invalid encodings",
          "[combat][encounter]") {
  Field_MpdEnemySpawn source{};
  source.facing = 2;
  VS::Game::EncounterSpawn spawn{0, {1, 2, 0, 0}};
  std::size_t index = 0;
  SECTION("object profile") { source.kind = 1; }
  SECTION("neutral profile") { source.facing = 0; }
  SECTION("friendly profile") { source.facing = 1; }
  SECTION("script-only profile") { source.facing = 3; }
  SECTION("wrong enemy") { source.enemyId = 1; }
  SECTION("negative coordinate") { spawn.position.x = -1; }
  SECTION("coordinate overflow") { spawn.position.z = 256; }
  SECTION("elevation overflow") { spawn.position.elevation = 4; }
  SECTION("direction overflow") { spawn.position.direction = 2; }
  SECTION("instance budget") { index = 8; }
  CHECK_FALSE(VS::Game::PrepareEncounterSpawn(source, spawn, index));
}

TEST_CASE("Encounter catalog hostility is independent of defeat retention",
          "[combat][encounter]") {
  Field_MpdEnemySpawn source{};
  source.actorStateByte = 1;
  SECTION("neutral") { source.facing = 0; }
  SECTION("friendly") { source.facing = 1; }
  SECTION("script-only") { source.facing = 3; }
  SECTION("object") {
    source.facing = 2;
    source.kind = 1;
  }
  CHECK_FALSE(VS::Game::IsEncounterHostileSpawn(source));
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

namespace {
struct FloorFixture {
  static constexpr std::size_t kPatterns = 8 + 4 * 2 * 2;
  std::vector<std::uint8_t> collision =
      std::vector<std::uint8_t>(kPatterns + std::size_t{2} * 16);
  std::array<Field_MpdTileCell, 4> cells{};

  FloorFixture() {
    // The disc header contains a profile count, not the live cell count.
    const Field_MpdCollisionHeader header{2, 2, 0, 2};
    std::memcpy(collision.data(), &header, sizeof(header));
    std::fill(collision.begin() + kPatterns + 16, collision.end(), 0x80);
  }

  auto Read() const {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(cells.data());
    return VS::Game::ReadEncounterFloor(collision, {bytes, sizeof(cells)});
  }
};
}  // namespace

TEST_CASE("Encounter floor decoding normalizes only its private header",
          "[combat][encounter][floor]") {
  FloorFixture fixture;
  SECTION("flat floor") {}
  SECTION("raised floor with valid negative native height") {
    const std::uint16_t descriptor = 0x800;
    std::memcpy(fixture.collision.data() + 8, &descriptor, sizeof(descriptor));
    std::fill_n(fixture.collision.begin() + FloorFixture::kPatterns, 16, 3);
  }
  const auto original = fixture.collision;
  const auto floor = fixture.Read();
  REQUIRE(floor.has_value());
  REQUIRE(floor->size() == 4);
  CHECK(std::ranges::all_of(*floor, [](const auto tile) {
    return tile == VS::Game::EncounterFloor::kSupported;
  }));
  CHECK(fixture.collision == original);
}

TEST_CASE("Encounter floor rejects blocked partial pit and invalid samples",
          "[combat][encounter][floor]") {
  FloorFixture fixture;
  auto expected = VS::Game::EncounterFloor::kBlocked;
  SECTION("blocked") { fixture.cells[0].doorPassability = 5; }
  SECTION("partial") { fixture.cells[0].doorPassability = 1; }
  SECTION("unsupported collision code") {
    fixture.cells[0].doorPassability = 7;
  }
  SECTION("pit") {
    fixture.cells[0].isPit = 1;
    expected = VS::Game::EncounterFloor::kPit;
  }
  SECTION("invalid floor pattern") {
    const std::uint16_t descriptor = 1;
    std::memcpy(fixture.collision.data() + 8, &descriptor, sizeof(descriptor));
    expected = VS::Game::EncounterFloor::kNoFloor;
  }
  SECTION("missing pattern") {
    const std::uint16_t descriptor = 0x7FF;
    std::memcpy(fixture.collision.data() + 8, &descriptor, sizeof(descriptor));
    expected = VS::Game::EncounterFloor::kNoFloor;
  }
  SECTION("invalid edge sample not just center") {
    fixture.collision[FloorFixture::kPatterns] = 0x80;
    expected = VS::Game::EncounterFloor::kNoFloor;
  }
  const auto floor = fixture.Read();
  REQUIRE(floor.has_value());
  CHECK((*floor)[0] == expected);
}

TEST_CASE("Encounter floor checks collision and tile record bounds",
          "[combat][encounter][floor]") {
  FloorFixture fixture;
  SECTION("header") { fixture.collision.resize(7); }
  SECTION("descriptor table") {
    fixture.collision.resize(FloorFixture::kPatterns - 1);
  }
  SECTION("unsupported dimensions") {
    const std::int16_t width = 33;
    std::memcpy(fixture.collision.data(), &width, sizeof(width));
  }
  CHECK_FALSE(fixture.Read().has_value());
  CHECK_FALSE(
      VS::Game::ReadEncounterFloor(FloorFixture{}.collision, {}).has_value());
}

TEST_CASE("Isolated collision sampling preserves layer bias and invalid bits",
          "[combat][encounter][floor]") {
  FloorFixture fixture;
  const std::int16_t live_count = 4;
  std::memcpy(fixture.collision.data() + 6, &live_count, sizeof(live_count));
  const std::uint16_t biased = 0x800;
  std::memcpy(fixture.collision.data() + 8, &biased, sizeof(biased));
  fixture.collision[FloorFixture::kPatterns] = 0x82;
  CHECK(Field_SampleCollisionHeightCodeFromData(fixture.collision.data(),
                                                fixture.collision.size(), 0, 0,
                                                0) == 0x86);
  CHECK(Field_SampleCollisionHeightCodeFromData(
            fixture.collision.data(), fixture.collision.size(), 1, 0, 0) == 4);
  CHECK(Field_SampleCollisionHeightCodeFromData(
            fixture.collision.data(), fixture.collision.size(), 1, 0, 1) == 0);
  CHECK(Field_SampleCollisionHeightCodeFromData(fixture.collision.data(),
                                                fixture.collision.size(), 8, 0,
                                                0) == 0x80);
  CHECK(Field_SampleCollisionHeightCodeFromData(nullptr, 0, 0, 0, 0) == 0x80);
}

TEST_CASE("Encounter validation identifies unsafe actor tiles before spawning",
          "[combat][encounter][floor]") {
  auto zone = TestZone();
  const auto setup = TestSetup(4);
  SECTION("missing floor data") { zone.rooms[0].floor.clear(); }
  SECTION("no floor under first enemy") {
    zone.rooms[0].floor[8 * 32 + 1] = VS::Game::EncounterFloor::kNoFloor;
  }
  SECTION("blocked fourth enemy") {
    zone.rooms[0].floor[8 * 32 + 4] = VS::Game::EncounterFloor::kBlocked;
  }
  SECTION("pit under Ashley") {
    zone.rooms[0].floor[2 * 32 + 4] = VS::Game::EncounterFloor::kPit;
  }
  const auto errors = ValidateEncounter(setup, zone);
  REQUIRE_FALSE(errors.empty());
  CHECK(errors.front().find("TILE (") != std::string::npos);
}
