#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "CombatLab/DoorTests.hpp"
#include "CombatLab/SceneCatalog.hpp"
#include "Content/DataAccess.h"
#include "Content/DataStore.hpp"

namespace {
VS::combat_lab::SceneOpcodes TestSceneOpcodes() {
  VS::combat_lab::SceneOpcodes result;
  result[1] = {1, VS::combat_lab::SceneFlow::kNext};
  result[2] = {4, VS::combat_lab::SceneFlow::kBranch, 2};
  result[3] = {3, VS::combat_lab::SceneFlow::kJump, 1};
  result[4] = {4, VS::combat_lab::SceneFlow::kLoop, 2};
  result[5] = {4, VS::combat_lab::SceneFlow::kThread, 2};
  result[6] = {1, VS::combat_lab::SceneFlow::kReturn};
  result[7] = {1, VS::combat_lab::SceneFlow::kFinish};
  result[8] = {1, VS::combat_lab::SceneFlow::kSpecial};
  result[9] = {3, VS::combat_lab::SceneFlow::kNext, 0,
               VS::combat_lab::SceneOperation::kChainEvent};
  result[10] = {10, VS::combat_lab::SceneFlow::kNext, 0,
                VS::combat_lab::SceneOperation::kChangeRoom};
  return result;
}

std::vector<std::uint8_t> Script(std::initializer_list<std::uint8_t> code) {
  std::vector<std::uint8_t> result(16);
  result.insert(result.end(), code);
  const auto end = static_cast<std::uint8_t>(result.size());
  result[2] = result[4] = result[6] = end;
  result.resize(result.size() + 16);
  return result;
}

void CheckSceneAssetIdentities(const nlohmann::json& inventory) {
  std::map<std::string, std::size_t> counts;
  std::set<std::string> paths;
  for (const auto& asset : inventory.at("assets")) {
    const auto path = asset.at("asset").get<std::string>();
    CHECK(paths.insert(path).second);
    ++counts[asset.at("kind").get<std::string>()];
    CHECK(asset.at("playback") == "unverified");
    CHECK(asset.at("preconditions") == "not_established");
  }
  CHECK(counts["room_events"] == 512);
  CHECK(counts["external_event"] == 512);
  CHECK(counts["actor_animation"] == 797);
  CHECK(counts["movie"] == 1);
}
}  // namespace

TEST_CASE("Scene fixtures use original supported non-door tiles",
          "[scenes][catalog][fixtures]") {
  VS::Game::EncounterRoom room;
  room.width = 3;
  room.depth = 2;
  room.floor.assign(6, VS::Game::EncounterFloor::kNoFloor);
  room.entrance = {1, 0, 0, 0};
  room.floor[1] = VS::Game::EncounterFloor::kSupported;
  room.floor[4] = VS::Game::EncounterFloor::kSupported;
  VS::Game::EncounterDoor door{};
  door.tile_x = 1;
  door.tile_z = 0;
  room.doors.push_back(door);
  const std::array<bool, 4> eligible = {
      VS::combat_lab::IsRoomFixtureTile(room, 1, 0),
      VS::combat_lab::IsRoomFixtureTile(room, -1, 0),
      VS::combat_lab::IsRoomFixtureTile(room, 3, 0),
      VS::combat_lab::IsRoomFixtureTile(room, 1, 1)};
  const std::array<bool, 4> expected = {false, false, false, true};
  CHECK(eligible == expected);
  const auto start = VS::combat_lab::FindRoomFixtureStart(room, room.entrance);
  if (!start.has_value()) {
    FAIL("Original supported non-door fixture was not found");
    return;
  }
  CHECK(start->x == 1);
  CHECK(start->z == 1);
  CHECK(room.floor[1] == VS::Game::EncounterFloor::kSupported);
  room.floor[4] = VS::Game::EncounterFloor::kNoFloor;
  CHECK_FALSE(
      VS::combat_lab::FindRoomFixtureStart(room, room.entrance).has_value());
  room.floor[4] = VS::Game::EncounterFloor::kSupported;
  room.unavailable_reason = "original room asset unavailable";
  CHECK_FALSE(
      VS::combat_lab::FindRoomFixtureStart(room, room.entrance).has_value());
}

TEST_CASE("Scene inventory distinguishes empty and malformed scripts",
          "[scenes][catalog]") {
  const auto opcodes = TestSceneOpcodes();
  CHECK(VS::combat_lab::DecodeSceneScript({}, opcodes).at("status") == "empty");
  const std::array<std::uint8_t, 32> zeros{};
  CHECK(VS::combat_lab::DecodeSceneScript(zeros, opcodes).at("status") ==
        "empty");
  CHECK_THROWS_AS(VS::combat_lab::DecodeSceneScript(
                      std::array<std::uint8_t, 1>{1}, opcodes),
                  std::runtime_error);
  auto script = Script({7});
  script[4] = 15;
  CHECK_THROWS_AS(VS::combat_lab::DecodeSceneScript(script, opcodes),
                  std::runtime_error);
}

TEST_CASE("Scene inventory retains both flag branches without executing them",
          "[scenes][catalog]") {
  const auto script = Script({2, 99, 5, 0, 7, 7});
  const auto decoded =
      VS::combat_lab::DecodeSceneScript(script, TestSceneOpcodes());
  CHECK(decoded.at("commands").size() == 3);
  CHECK(decoded.at("edges").size() == 2);
  CHECK(decoded.at("commands").at(0).at("operands") ==
        nlohmann::json::array({99, 5, 0}));
  CHECK(decoded.at("playback") == "unverified");
  CHECK(decoded.at("preconditions") == "not_established");
}

TEST_CASE("Scene inventory preserves loops and child threads",
          "[scenes][catalog]") {
  const auto opcodes = TestSceneOpcodes();
  const auto loop =
      VS::combat_lab::DecodeSceneScript(Script({1, 4, 0, 1, 0, 7}), opcodes);
  CHECK(loop.at("commands").size() == 3);
  CHECK(loop.at("edges").at(1).at("kind") == "loop");
  CHECK(loop.at("edges").at(1).at("to") == 16);
  const auto thread =
      VS::combat_lab::DecodeSceneScript(Script({5, 1, 5, 0, 7, 7}), opcodes);
  CHECK(thread.at("commands").size() == 3);
  CHECK(thread.at("edges").at(0).at("kind") == "parent_thread");
  CHECK(thread.at("edges").at(1).at("kind") == "child_thread");
  const auto backward =
      VS::combat_lab::DecodeSceneScript(Script({1, 3, 255, 255}), opcodes);
  CHECK(backward.at("commands").size() == 2);
  CHECK(backward.at("edges").at(1).at("to") == 16);
}

TEST_CASE("Scene inventory reports dynamic returns and special preconditions",
          "[scenes][catalog]") {
  const auto decoded =
      VS::combat_lab::DecodeSceneScript(Script({8, 6}), TestSceneOpcodes());
  REQUIRE(decoded.at("unresolved").size() == 2);
  CHECK(decoded.at("unresolved").at(0).at("kind") ==
        "special_operation_preconditions");
  CHECK(decoded.at("unresolved").at(1).at("kind") == "saved_return_cursor");
}

TEST_CASE(
    "Scene inventory links chained events without inventing target validity",
    "[scenes][catalog]") {
  const auto opcodes = TestSceneOpcodes();
  const auto chained =
      VS::combat_lab::DecodeSceneScript(Script({9, 81, 0, 7}), opcodes);
  CHECK(chained.at("commands").at(0).at("operation") == "load_next_event");
  CHECK(chained.at("commands").at(0).at("event_id") == 81);
  const auto invalid =
      VS::combat_lab::DecodeSceneScript(Script({9, 255, 255, 7}), opcodes);
  CHECK(invalid.at("commands").at(0).at("event_id") == -1);
}

TEST_CASE("Scene inventory decodes current-zone room handoffs losslessly",
          "[scenes][catalog]") {
  const auto decoded = VS::combat_lab::DecodeSceneScript(
      Script({10, 0, 21, 3, 7, 5, 2, 4, 0, 60, 7}), TestSceneOpcodes());
  const auto& command = decoded.at("commands").at(0);
  CHECK(command.at("operation") == "change_room");
  CHECK(command.at("destination") ==
        nlohmann::json{{"zone", 0}, {"room", 21}, {"zone_mode", "current"}});
  CHECK(command.at("entry") ==
        nlohmann::json{{"x", 3}, {"z", 5}, {"direction", 7}, {"elevation", 2}});
  CHECK(command.at("flags") == 4);
  CHECK(command.at("transition") ==
        nlohmann::json{{"effect", 0}, {"speed", 60}});
}

TEST_CASE("Scene inventory preserves explicit cross-zone destinations",
          "[scenes][catalog]") {
  const auto decoded = VS::combat_lab::DecodeSceneScript(
      Script({10, 40, 21, 3, 7, 5, 2, 4, 0, 60, 7}), TestSceneOpcodes());
  CHECK(decoded.at("commands").at(0).at("destination") ==
        nlohmann::json{{"zone", 40}, {"room", 21}, {"zone_mode", "explicit"}});
}

TEST_CASE("Scene transition operands cannot escape their instruction",
          "[scenes][catalog]") {
  auto opcodes = TestSceneOpcodes();
  opcodes[9].size = 1;
  CHECK_THROWS_AS(VS::combat_lab::DecodeSceneScript(Script({9, 7}), opcodes),
                  std::runtime_error);
  opcodes[10].size = 2;
  CHECK_THROWS_AS(
      VS::combat_lab::DecodeSceneScript(Script({10, 0, 7}), opcodes),
      std::runtime_error);
}

TEST_CASE(
    "Scene inventory rejects invalid control flow rather than skipping it",
    "[scenes][catalog]") {
  const auto opcodes = TestSceneOpcodes();
  for (const auto& script :
       {Script({0}), Script({3, 100, 0}), Script({2, 0, 2, 0, 7}),
        Script({4, 0, 99, 0}), Script({1})}) {
    CHECK_THROWS_AS(VS::combat_lab::DecodeSceneScript(script, opcodes),
                    std::runtime_error);
  }
}

TEST_CASE("Scene trigger inventory supports short sentinel tables",
          "[scenes][catalog]") {
  const std::array<std::uint8_t, 4> empty{255, 255, 0, 0};
  CHECK(VS::combat_lab::DecodeSceneTriggers(empty).empty());
  const std::array<std::uint8_t, 8> short_table{4, 0, 255, 255, 1, 80, 0, 255};
  const auto external = VS::combat_lab::DecodeSceneTriggers(short_table);
  REQUIRE(external.size() == 1);
  CHECK(external.at(0).at("source") == "external");
  CHECK(external.at(0).at("selector") == 80);
}

TEST_CASE("Scene trigger inventory retains gates, negation and event identity",
          "[scenes][catalog]") {
  std::vector<std::uint8_t> triggers(32, 255);
  triggers[0] = 32;
  triggers[1] = 0;
  triggers.insert(triggers.end(), {10, 3, 128, 254, 3, 32, 4, 5, 255});
  const auto decoded = VS::combat_lab::DecodeSceneTriggers(triggers);
  REQUIRE(decoded.size() == 1);
  const auto& trigger = decoded.at(0);
  CHECK(trigger.at("type_mask") == 10);
  CHECK(trigger.at("source") == "room_local");
  CHECK(trigger.at("selector") == 3);
  REQUIRE(trigger.at("conditions").size() == 2);
  CHECK(trigger.at("conditions").at(0).at("opcode") == 254);
  CHECK(trigger.at("conditions").at(1).at("operands") ==
        nlohmann::json::array({32, 4, 5}));
}

TEST_CASE(
    "Scene trigger inventory fails explicitly for truncated or unknown data",
    "[scenes][catalog]") {
  CHECK_THROWS_AS(VS::combat_lab::DecodeSceneTriggers({}), std::runtime_error);
  std::vector<std::uint8_t> triggers(32, 255);
  triggers[0] = 32;
  triggers[1] = 0;
  triggers.insert(triggers.end(), {1, 80, 0, 17});
  CHECK_THROWS_AS(VS::combat_lab::DecodeSceneTriggers(triggers),
                  std::runtime_error);
  triggers.back() = 1;
  CHECK_THROWS_AS(VS::combat_lab::DecodeSceneTriggers(triggers),
                  std::runtime_error);
}

TEST_CASE("Scene trigger offsets cannot alias their own offset table",
          "[scenes][catalog]") {
  const std::array<std::uint8_t, 4> aliased{0, 0, 255, 255};
  CHECK_THROWS_AS(VS::combat_lab::DecodeSceneTriggers(aliased),
                  std::runtime_error);
}

TEST_CASE("Scene inventory covers every original event and animation asset",
          "[.assets][disc-scene-inventory]") {
  VS::Content::DataStore::ScopedOverride restore;
  REQUIRE(VS::Content::DataStore::Open("Native\\data").has_value());
  REQUIRE(Content_LoadModuleData() == 0);
  const auto catalog = VS::Game::ReadEncounterCatalog(true);
  REQUIRE(catalog.has_value());
  const auto inventory = VS::combat_lab::ReadSceneCatalog(*catalog);
  CHECK(inventory.at("coverage") == "static_scene_inventory");
  CHECK(inventory.at("errors") == 0);
  CheckSceneAssetIdentities(inventory);
  CHECK(inventory == VS::combat_lab::ReadSceneCatalog(*catalog));
}
