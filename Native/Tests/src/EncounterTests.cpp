#include <algorithm>
#include <array>
#include <cstring>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Encounter.hpp"
#include "CombatLab/Editor.hpp"
#include "CombatLab/Trace.hpp"
#include "Content/DataStore.hpp"
#include "Core/BiosRand.h"
#include "Core/Random.hpp"
#include "Field/DoorMatches.h"
#include "Field/EncounterSpawnProfile.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/EncounterHooks.h"
#include "Boot/PlayerSettings.h"
#include "Field/CollisionSampling.h"
#include "Field/Gimmicks.h"
#include "Field/Room.h"
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
           std::size_t{32} * 32, VS::Game::EncounterFloor::kSupported),
       {}});
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

void EditorPress(VS::combat_lab::Editor& editor, std::uint32_t button,
                 int times = 1) {
  VS::Input::PadState pad{};
  pad.pressed = button;
  for (int index = 0; index < times; ++index) editor.Update(pad, false);
}

struct DoorQueryFixture {
  Field_RoomRecord saved_room = Field_roomData;
  char saved_random = Audio_Seq_unusedStateFlags.opaqueAC[7];
  std::array<Field_MpdDoorRecord, 40> doors{};

  DoorQueryFixture() {
    Field_roomData = {};
    Field_roomData.doors = doors.data();
    Field_roomData.header.doorsLength = sizeof(doors);
  }

  ~DoorQueryFixture() {
    Field_roomData.doors = nullptr;
    Field_TransitionDoorsAtTile(0, 0, 0);
    Field_roomData = saved_room;
    Audio_Seq_unusedStateFlags.opaqueAC[7] = saved_random;
    Core_Random_Seed(1);
  }

  void Check(int x, int z, int layer, std::span<const int> expected) {
    auto* result = Field_TransitionDoorsAtTile(x, z, layer);
    REQUIRE(result == &Field_matchingDoorRecords);
    REQUIRE(result->count == static_cast<int>(expected.size()));
    for (std::size_t index = 0; index < expected.size(); ++index) {
      CAPTURE(index);
      CHECK(result->records[index] == &doors[expected[index]]);
    }
  }
};
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

TEST_CASE("Configured enemy placements belong only to their source room",
          "[combat][encounter][placements]") {
  for (int count : {0, 1, 8}) {
    const auto setup = TestSetup(count);
    CAPTURE(count);
    CHECK(VS::Game::UsesConfiguredEncounterPlacements(setup, 9, 0));
    CHECK_FALSE(VS::Game::UsesConfiguredEncounterPlacements(setup, 42, 0));
    CHECK_FALSE(VS::Game::UsesConfiguredEncounterPlacements(setup, 9, 1));
    CHECK_FALSE(VS::Game::UsesConfiguredEncounterPlacements(setup, 42, 1));
    CHECK(VS::Game::UsesConfiguredEncounterPlacements(setup, 9, 0));
  }
}

TEST_CASE("Inactive encounter hooks preserve original room placements",
          "[combat][encounter][placements]") {
  std::array<Field_MpdEnemySpawn, 2> original{};
  void* data = original.data();
  unsigned int size = sizeof(original);
  Boot_EncounterRoomPlacements(&data, &size);
  CHECK(data == original.data());
  CHECK(size == sizeof(original));
  data = nullptr;
  size = 0;
  Boot_EncounterRoomPlacements(&data, &size);
  CHECK(data == nullptr);
  CHECK(size == 0);
}

TEST_CASE("Door query results have actual native pointer-list storage",
          "[field][doors][storage]") {
  CHECK(std::is_pointer_v<decltype(Field_DoorMatchList::records)>);
  CHECK(sizeof(Field_DoorMatchList) >=
        sizeof(int) + sizeof(Field_MpdDoorRecord*));
}

TEST_CASE("Door queries preserve ordered tile and platform matches on repeat",
          "[field][doors][storage]") {
  DoorQueryFixture fixture;
  fixture.doors[0].tileLocation.tileX = fixture.doors[1].tileLocation.tileX = 4;
  fixture.doors[0].tileLocation.tileZ = fixture.doors[1].tileLocation.tileZ = 7;
  fixture.doors[2].tileLocation.tileX = 8;
  fixture.doors[2].tileLocation.tileZ = 9;
  fixture.doors[3].tileLocation = fixture.doors[0].tileLocation;
  fixture.doors[3].tileLocation.floorLayer = 1;
  for (int repeat = 0; repeat < 2; ++repeat) {
    fixture.Check(4, 7, 0, std::array{0, 1});
    fixture.Check(8, 9, 0, std::array{2});
    fixture.Check(4, 7, 1, std::array{3});
    fixture.Check(31, 31, 0, std::span<const int>{});
  }
  std::array<int, 40> expected{};
  for (std::size_t index = 0; index < fixture.doors.size(); ++index) {
    auto& tile = fixture.doors[index].tileLocation;
    tile.tileX = static_cast<u_short>(index % 32);
    tile.tileZ = static_cast<u_short>(index / 32);
    tile.floorLayer = 2;
    expected[index] = static_cast<int>(index);
  }
  fixture.Check(-1, -1, 2, expected);
  fixture.Check(0, 0, 3, std::span<const int>{});
  fixture.Check(0, 0, 2, expected);
}

TEST_CASE("Door queries retain raw RNG semantics and reject malformed data",
          "[field][doors][storage]") {
  DoorQueryFixture fixture;
  Core_Random_Seed(1234);
  VS::Core::Rand15 expected(1234);
  SECTION("present room consumes one raw draw even without a match") {
    fixture.Check(31, 31, 0, std::span<const int>{});
    CHECK(static_cast<unsigned char>(Audio_Seq_unusedStateFlags.opaqueAC[7]) ==
          static_cast<unsigned char>(expected.Next()));
  }
  SECTION("absent room consumes no raw draw") {
    Field_roomData.doors = nullptr;
    CHECK(Field_TransitionDoorsAtTile(0, 0, 0) == nullptr);
    CHECK(Field_matchingDoorRecords.count == 0);
  }
  SECTION("partial record bytes are rejected before traversal") {
    --Field_roomData.header.doorsLength;
    CHECK(Field_TransitionDoorsAtTile(0, 0, 0) == nullptr);
    CHECK(Field_matchingDoorRecords.count == 0);
  }
  CHECK(Core_Random_Rand() == expected.Next());
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
  CHECK_FALSE(ValidateEncounter(setup, zone).empty());
}

TEST_CASE("Solo encounters retain Ashley placement and profile validation",
          "[combat][encounter]") {
  const auto zone = TestZone();
  auto setup = TestSetup(0);
  CHECK(ValidateEncounter(setup, zone).empty());
  setup.profile = VS::Game::EncounterProfile::kAllItemsB;
  CHECK(ValidateEncounter(setup, zone).empty());
  setup.profile = VS::Game::EncounterProfile::kAllSpells;
  CHECK(ValidateEncounter(setup, zone).empty());
  setup.profile = VS::Game::EncounterProfile::kAllAbilities;
  CHECK(ValidateEncounter(setup, zone).empty());
  setup.profile = VS::Game::EncounterProfile::kAllBreakArts;
  CHECK(ValidateEncounter(setup, zone).empty());
  setup.player.x = -1;
  CHECK_FALSE(ValidateEncounter(setup, zone).empty());
  setup.player.x = 4;
  setup.profile = VS::Game::EncounterProfile::kCount;
  CHECK_FALSE(ValidateEncounter(setup, zone).empty());
}

TEST_CASE("Combat Lab editor preserves solo presets and group selection",
          "[combat][encounter][editor]") {
  VS::combat_lab::Editor editor({TestZone()});
  CHECK(editor.Setup().enemies.size() == 1);
  SECTION("quantity zero and room reset") {
    EditorPress(editor, VS::Input::Down, 3);
    EditorPress(editor, VS::Input::Left, 2);
    CHECK(editor.Setup().enemies.empty());
    EditorPress(editor, VS::Input::Up, 2);
    EditorPress(editor, VS::Input::Right);
    CHECK(editor.Setup().enemies.empty());
  }
  SECTION("explicit groups are not replaced by quantity zero") {
    editor.Load(TestSetup(2));
    EditorPress(editor, VS::Input::Down, 3);
    EditorPress(editor, VS::Input::Left);
    CHECK(editor.Setup().enemies.size() == 2);
    EditorPress(editor, VS::Input::Down, 11);
    EditorPress(editor, VS::Input::Circle);
    CHECK(editor.Setup().enemies.empty());
  }
}

TEST_CASE("Original-room editor cannot replace production actors with groups",
          "[combat][encounter][editor][doors]") {
  VS::combat_lab::Editor editor({TestZone()});
  auto setup = TestSetup(0);
  setup.room_policy = VS::Game::EncounterRoomPolicy::kOriginalRoom;
  editor.Load(setup);
  EditorPress(editor, VS::Input::Down, 3);
  EditorPress(editor, VS::Input::Right, 2);
  CHECK(editor.Setup().enemies.empty());
  EditorPress(editor, VS::Input::Down, 10);
  EditorPress(editor, VS::Input::Circle);
  CHECK(editor.Setup().enemies.empty());
  EditorPress(editor, VS::Input::Down);
  EditorPress(editor, VS::Input::Circle);
  CHECK(editor.Setup().enemies.empty());
  CHECK(editor.Setup().room_policy ==
        VS::Game::EncounterRoomPolicy::kOriginalRoom);
  CHECK(VS::Game::ValidateEncounter(editor.Setup(), editor.Zone()).empty());
}

TEST_CASE("Combat Lab editor selects and wraps the all-action profiles",
          "[combat][encounter][editor][spells][abilities][break-arts]") {
  VS::combat_lab::Editor editor({TestZone()});
  EditorPress(editor, VS::Input::Down, 11);
  for (const auto profile : {VS::Game::EncounterProfile::kAllBreakArts,
                             VS::Game::EncounterProfile::kAllAbilities,
                             VS::Game::EncounterProfile::kAllSpells}) {
    EditorPress(editor, VS::Input::Left);
    CHECK(editor.Setup().profile == profile);
    const auto setup = editor.Setup();
    editor.Load(
        VS::combat_lab::DecodeSetup(VS::combat_lab::EncodeSetup(setup)));
    CHECK(editor.Setup().profile == profile);
  }
  for (const auto profile : {VS::Game::EncounterProfile::kAllAbilities,
                             VS::Game::EncounterProfile::kAllBreakArts,
                             VS::Game::EncounterProfile::kPrologue}) {
    EditorPress(editor, VS::Input::Right);
    CHECK(editor.Setup().profile == profile);
  }
}

TEST_CASE("Combat Lab editor loads empty and populated presets",
          "[combat][encounter][editor]") {
  VS::combat_lab::Editor editor({TestZone()});
  SECTION("loaded solo survives save, group add and clear") {
    auto setup = TestSetup(0);
    setup.profile = VS::Game::EncounterProfile::kAllItemsB;
    editor.Load(
        VS::combat_lab::DecodeSetup(VS::combat_lab::EncodeSetup(setup)));
    CHECK(editor.Setup().enemies.empty());
    CHECK(editor.Setup().profile == setup.profile);
    EditorPress(editor, VS::Input::Down, 13);
    EditorPress(editor, VS::Input::Circle);
    CHECK(editor.Setup().enemies.empty());
    EditorPress(editor, VS::Input::Down);
    EditorPress(editor, VS::Input::Circle);
    CHECK(editor.Setup().enemies.empty());
  }
  SECTION("non-solo loading restores current selection after clearing") {
    editor.Load(TestSetup(0));
    editor.Load(TestSetup(2));
    EditorPress(editor, VS::Input::Down, 14);
    EditorPress(editor, VS::Input::Circle);
    CHECK(editor.Setup().enemies.size() == 1);
  }
}

TEST_CASE("Combat Lab Ashley stats editor preserves defaults and limits",
          "[combat][encounter][editor][stats]") {
  VS::combat_lab::Editor editor({TestZone()});
  EditorPress(editor, VS::Input::Down, 12);
  EditorPress(editor, VS::Input::Circle);
  EditorPress(editor, VS::Input::Left);
  CHECK(editor.Setup().stat_overrides.max_hp == 999);
  EditorPress(editor, VS::Input::Right);
  CHECK(editor.Setup().stat_overrides.max_hp == 999);
  EditorPress(editor, VS::Input::Down);
  EditorPress(editor, VS::Input::Left);
  CHECK(editor.Setup().stat_overrides.max_mp == 999);
  VS::Input::PadState pad{};
  pad.pressed = VS::Input::Left;
  pad.held = VS::Input::R1;
  editor.Update(pad, false);
  CHECK(editor.Setup().stat_overrides.max_mp == 899);
  pad.held = VS::Input::L1;
  editor.Update(pad, false);
  CHECK(editor.Setup().stat_overrides.max_mp == 889);
  CHECK_FALSE(editor.Setup().stat_overrides.strength.has_value());
}

TEST_CASE("Combat Lab Ashley stats editor saves and clears overrides",
          "[combat][encounter][editor][stats]") {
  VS::combat_lab::Editor editor({TestZone()});
  auto setup = TestSetup();
  setup.stat_overrides.max_hp = 999;
  setup.stat_overrides.max_mp = 889;
  editor.Load(setup);
  EditorPress(editor, VS::Input::Down, 12);
  EditorPress(editor, VS::Input::Circle);
  EditorPress(editor, VS::Input::Down);
  EditorPress(editor, VS::Input::Circle);
  CHECK_FALSE(editor.Setup().stat_overrides.max_mp.has_value());
  EditorPress(editor, VS::Input::Right);
  EditorPress(editor, VS::Input::Left);
  CHECK(editor.Setup().stat_overrides.max_mp == 1);
  EditorPress(editor, VS::Input::Cross);
  EditorPress(editor, VS::Input::Down, 3);
  CHECK(editor.Update({}, true) == VS::combat_lab::Editor::Action::kStart);
  const auto saved =
      VS::combat_lab::DecodeSetup(VS::combat_lab::EncodeSetup(editor.Setup()));
  editor.Load(saved);
  CHECK(editor.Setup().stat_overrides == saved.stat_overrides);
  EditorPress(editor, VS::Input::Up, 3);
  EditorPress(editor, VS::Input::Circle);
  EditorPress(editor, VS::Input::Down, 6);
  EditorPress(editor, VS::Input::Circle);
  CHECK(editor.Setup().stat_overrides == VS::Game::EncounterStatOverrides{});
  EditorPress(editor, VS::Input::Down);
  EditorPress(editor, VS::Input::Circle);
  CHECK(ValidateEncounter(editor.Setup(), editor.Zone()).empty());
}

TEST_CASE("Encounter setup validates every Ashley stat override",
          "[combat][encounter][stats]") {
  const auto zone = TestZone();
  auto setup = TestSetup(0);
  using Stats = VS::Game::EncounterStatOverrides;
  for (const auto member : {&Stats::max_hp, &Stats::max_mp, &Stats::strength,
                            &Stats::intelligence, &Stats::agility}) {
    for (const int value : {-1, 0, 1000}) {
      setup.stat_overrides = {};
      setup.stat_overrides.*member = value;
      CHECK_FALSE(ValidateEncounter(setup, zone).empty());
    }
  }
  setup.stat_overrides = {999, 999, 999, 999, 999};
  CHECK(ValidateEncounter(setup, zone).empty());
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

TEST_CASE("Combat Lab weapon selection wraps through all families",
          "[combat][encounter][editor][weapon]") {
  using Weapon = VS::Game::EncounterWeapon;
  VS::combat_lab::Editor editor({TestZone()});
  EditorPress(editor, VS::Input::Down, 12);
  EditorPress(editor, VS::Input::Circle);
  EditorPress(editor, VS::Input::Down, 5);
  EditorPress(editor, VS::Input::Left);
  CHECK(editor.Setup().weapon == Weapon::kBareHands);
  EditorPress(editor, VS::Input::Right);
  CHECK(editor.Setup().weapon == Weapon::kDefault);
  EditorPress(editor, VS::Input::Right, 3);
  CHECK(editor.Setup().weapon == Weapon::kGreatsword);
}

TEST_CASE("Combat Lab weapon presets retain and reset equipment choices",
          "[combat][encounter][editor][weapon]") {
  using Weapon = VS::Game::EncounterWeapon;
  VS::combat_lab::Editor editor({TestZone()});
  auto setup = editor.Setup();
  setup.weapon = Weapon::kGreatsword;
  editor.Load(setup);
  EditorPress(editor, VS::Input::Down, 15);
  CHECK(editor.Update({}, true) == VS::combat_lab::Editor::Action::kStart);
  editor.Load(
      VS::combat_lab::DecodeSetup(VS::combat_lab::EncodeSetup(editor.Setup())));
  CHECK(editor.Setup().weapon == Weapon::kGreatsword);
  EditorPress(editor, VS::Input::Up, 3);
  EditorPress(editor, VS::Input::Circle);
  EditorPress(editor, VS::Input::Down, 5);
  EditorPress(editor, VS::Input::Circle);
  CHECK(editor.Setup().weapon == Weapon::kDefault);
  EditorPress(editor, VS::Input::Right);
  EditorPress(editor, VS::Input::Down);
  EditorPress(editor, VS::Input::Circle);
  CHECK(editor.Setup().weapon == Weapon::kDefault);
}

TEST_CASE("Encounter validation rejects unsupported equipment choices",
          "[combat][encounter][weapon]") {
  auto setup = TestSetup(0);
  setup.weapon = VS::Game::EncounterWeapon::kCount;
  CHECK_FALSE(ValidateEncounter(setup, TestZone()).empty());
}

TEST_CASE("Production room presets retain original actors and event policy",
          "[combat][encounter][doors][production]") {
  auto setup = TestSetup(0);
  setup.room_policy = VS::Game::EncounterRoomPolicy::kOriginalRoom;
  CHECK(ValidateEncounter(setup, TestZone()).empty());
  CHECK_FALSE(VS::Game::UsesConfiguredEncounterPlacements(setup, 9, 0));
  const auto encoded = VS::combat_lab::EncodeSetup(setup);
  CHECK(encoded.at("room_events") == "production");
  const auto restored = VS::combat_lab::DecodeSetup(encoded);
  CHECK(restored.room_policy == setup.room_policy);
  setup.enemies = TestSetup().enemies;
  CHECK_FALSE(ValidateEncounter(setup, TestZone()).empty());
  auto invalid = encoded;
  invalid["room_events"] = "unknown";
  CHECK_THROWS(VS::combat_lab::DecodeSetup(invalid));
}

TEST_CASE("External event components roundtrip without changing older presets",
          "[combat][encounter][event-component]") {
  auto setup = TestSetup(0);
  const auto original = VS::combat_lab::EncodeSetup(setup);
  CHECK_FALSE(original.contains("external_event"));
  CHECK_FALSE(VS::combat_lab::DecodeSetup(original).external_event);
  setup.external_event = 82;
  REQUIRE(ValidateEncounter(setup, TestZone()).empty());
  const auto encoded = VS::combat_lab::EncodeSetup(setup);
  CHECK(encoded.at("external_event") == 82);
  const auto restored = VS::combat_lab::DecodeSetup(encoded);
  CHECK(restored.external_event == setup.external_event);
  CHECK(restored.room_policy == VS::Game::EncounterRoomPolicy::kIsolatedCombat);
  CHECK(restored.enemies.empty());
}

TEST_CASE(
    "External event components reject ambiguous entry and actor ownership",
    "[combat][encounter][event-component]") {
  auto setup = TestSetup(0);
  setup.external_event = 82;
  setup.room_policy = VS::Game::EncounterRoomPolicy::kOriginalRoom;
  CHECK_FALSE(ValidateEncounter(setup, TestZone()).empty());
  setup.room_policy = VS::Game::EncounterRoomPolicy::kIsolatedCombat;
  setup.enemies = TestSetup(1).enemies;
  CHECK_FALSE(ValidateEncounter(setup, TestZone()).empty());
  setup.enemies.clear();
  setup.external_event = -1;
  CHECK_FALSE(ValidateEncounter(setup, TestZone()).empty());
  setup.external_event = 10000;
  CHECK_FALSE(ValidateEncounter(setup, TestZone()).empty());
  setup.external_event = 0;
  CHECK(ValidateEncounter(setup, TestZone()).empty());
}

TEST_CASE("External event preset numbers are explicit integers",
          "[combat][encounter][event-component]") {
  auto preset = VS::combat_lab::EncodeSetup(TestSetup(0));
  for (const auto& invalid :
       {nlohmann::json(nullptr), nlohmann::json("82"), nlohmann::json(true),
        nlohmann::json(82.5), nlohmann::json(-1), nlohmann::json(10000)}) {
    preset["external_event"] = invalid;
    CHECK_THROWS(VS::combat_lab::DecodeSetup(preset));
  }
}

TEST_CASE("External event editor retains script-owned actors",
          "[combat][encounter][event-component][editor]") {
  VS::combat_lab::Editor editor({TestZone()});
  auto setup = TestSetup(0);
  setup.external_event = 82;
  editor.Load(setup);
  EditorPress(editor, VS::Input::Down, 3);
  EditorPress(editor, VS::Input::Right, 2);
  CHECK(editor.Setup().enemies.empty());
  CHECK(editor.Setup().external_event == 82);
  CHECK(ValidateEncounter(editor.Setup(), editor.Zone()).empty());
}

TEST_CASE("External event components fail preflight without original data",
          "[combat][encounter][event-component]") {
  VS::Content::DataStore::ScopedOverride content;
  auto setup = TestSetup(0);
  setup.external_event = 82;
  const auto configured = VS::Game::ConfigureEncounter(setup, TestZone());
  REQUIRE_FALSE(configured);
  CHECK(configured.error().find("External event is unavailable") !=
        std::string::npos);
}

TEST_CASE(
    "External components preserve production handoffs after isolated entry",
    "[combat][encounter][event-component]") {
  auto setup = TestSetup(0);
  CHECK_FALSE(VS::Game::UsesEncounterRoomEvents(setup, false));
  CHECK_FALSE(VS::Game::UsesEncounterRoomEvents(setup, true));
  CHECK(VS::Game::UsesConfiguredEncounterPlacements(setup, 9, 0, true));
  setup.external_event = 80;
  CHECK_FALSE(VS::Game::UsesEncounterRoomEvents(setup, false));
  CHECK(VS::Game::UsesConfiguredEncounterPlacements(setup, 9, 0, false));
  CHECK(VS::Game::UsesEncounterRoomEvents(setup, true));
  CHECK_FALSE(VS::Game::UsesConfiguredEncounterPlacements(setup, 9, 0, true));
  CHECK_FALSE(VS::Game::UsesConfiguredEncounterPlacements(setup, 19, 0, true));
  setup.external_event.reset();
  setup.room_policy = VS::Game::EncounterRoomPolicy::kOriginalRoom;
  CHECK(VS::Game::UsesEncounterRoomEvents(setup, false));
  CHECK(VS::Game::UsesEncounterRoomEvents(setup, true));
}
