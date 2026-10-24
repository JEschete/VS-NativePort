#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Encounter.hpp"
#include "CombatLab/DoorTests.hpp"
#include "Content/DataStore.hpp"
#include "Input/Pad.hpp"

namespace {
using VS::combat_lab::DoorTrial;
using VS::Game::EncounterDoorRequestStatus;

std::vector<VS::Game::EncounterZone> TestDoorCatalog() {
  VS::Game::EncounterRoom room;
  room.index = 0;
  room.asset = "shared-room";
  room.width = room.depth = 3;
  room.floor.assign(9, VS::Game::EncounterFloor::kSupported);
  VS::Game::EncounterDoor door;
  door.destination_zone = 9;
  door.destination_room = 0;
  door.tile_x = door.tile_z = 1;
  door.destination = {2, 1, 3, 7};
  door.required_flag = 1023;
  door.required_value = 255;
  door.entry_step = true;
  room.doors.push_back(door);
  VS::Game::EncounterZone zone;
  zone.id = 9;
  zone.rooms.push_back(room);
  auto other = zone;
  other.id = 42;
  return {zone, other};
}

VS::Game::EncounterState DoorSource() {
  VS::Game::EncounterState state;
  state.ready = true;
  state.zone = 9;
  state.room = 0;
  state.control_mode = 1;
  return state;
}

DoorTrial RequestedDoor() {
  auto test = VS::combat_lab::EnumerateDoorTests(TestDoorCatalog()).front();
  test.door.destination_room = 1;
  DoorTrial trial(test);
  REQUIRE(trial.Observe(DoorSource(), true, 1));
  trial.Requested({EncounterDoorRequestStatus::kAccepted, {}}, 2);
  auto loading = DoorSource();
  loading.ready = false;
  CHECK_FALSE(trial.Observe(loading, false, 3));
  return trial;
}

std::uint32_t ReadDoorInteger(std::span<const std::uint8_t> bytes,
                              std::size_t offset, unsigned width) {
  REQUIRE(offset <= bytes.size());
  REQUIRE(width <= bytes.size() - offset);
  std::uint32_t value = 0;
  REQUIRE(width <= sizeof(value));
  for (unsigned i = 0; i < width; ++i) {
    value |= static_cast<std::uint32_t>(bytes[offset + i]) << (i * 8);
  }
  return value;
}

void CheckOriginalDoorIdentity(std::span<const std::uint8_t> bytes,
                               const VS::Game::EncounterDoor& door) {
  const auto tile = ReadDoorInteger(bytes, 2, 2);
  const auto entry = ReadDoorInteger(bytes, 4, 2);
  const auto flag = ReadDoorInteger(bytes, 6, 2);
  CHECK(door.destination_zone == bytes[0]);
  CHECK(door.destination_room == bytes[1]);
  CHECK(door.tile_x == (tile & 31));
  CHECK(door.tile_z == ((tile >> 5) & 31));
  CHECK(door.layer == ((tile >> 10) & 7));
  CHECK(door.required_value == (entry & 255));
  CHECK(door.required_flag == (flag & 1023));
}

void CheckOriginalDoorEntry(std::span<const std::uint8_t> bytes,
                            const VS::Game::EncounterDoor& door) {
  const auto tile = ReadDoorInteger(bytes, 2, 2);
  const auto entry = ReadDoorInteger(bytes, 4, 2);
  const auto flag = ReadDoorInteger(bytes, 6, 2);
  CHECK(door.destination.direction == ((tile >> 13) & 7));
  CHECK(door.destination.x == ((entry >> 8) & 31));
  CHECK(door.destination.elevation == ((entry >> 13) & 3));
  CHECK(door.entry_step == ((entry >> 15) != 0));
  CHECK(door.destination.z == ((flag >> 10) & 31));
  CHECK(door.door_id == bytes[8]);
}

void CheckDoorFixture(const VS::combat_lab::DoorTestCase& test) {
  CHECK(test.unavailable.empty());
  CHECK(test.setup.enemies.empty());
  CHECK(test.setup.room_policy == VS::Game::EncounterRoomPolicy::kOriginalRoom);
  CHECK_FALSE((test.setup.player.x == 1 && test.setup.player.z == 1));
}

void CheckDoorMetadata(const VS::combat_lab::DoorTestCase& test) {
  const auto encoded = VS::combat_lab::EncodeDoorTest(test);
  CHECK(encoded.at("required_flag") == 1023);
  CHECK(encoded.at("required_value") == 255);
  CHECK(encoded.at("entry") == nlohmann::json::array({2, 1, 3, 7}));
  CHECK(encoded.at("entry_step") == true);
}

void ObserveDoorWaiting(DoorTrial& trial, const VS::Game::EncounterState& state,
                        bool idle, std::uint64_t tick) {
  CHECK_FALSE(trial.Observe(state, idle, tick));
  CHECK_FALSE(trial.Finished());
}

using DoorCase = VS::combat_lab::DoorTestCase;
using DoorCaseIndex = std::map<std::tuple<int, int, int>, const DoorCase*>;

std::size_t CheckRawRoomDoors(std::span<const std::uint8_t> bytes, int zone,
                              int room, const DoorCaseIndex& indexed) {
  const auto section = ReadDoorInteger(bytes, 0, 4);
  std::size_t doors = static_cast<std::size_t>(section) + 96;
  for (std::size_t i = 0; i < 3; ++i) {
    doors += ReadDoorInteger(bytes, section + i * 4, 4);
  }
  const auto length = ReadDoorInteger(bytes, section + 12, 4);
  REQUIRE(length % 12 == 0);
  REQUIRE(doors <= bytes.size());
  REQUIRE(length <= bytes.size() - doors);
  for (std::size_t index = 0; index < length / 12; ++index) {
    const auto found =
        indexed.find(std::tuple(zone, room, static_cast<int>(index)));
    REQUIRE(found != indexed.end());
    const auto record = bytes.subspan(doors + index * 12, 12);
    CheckOriginalDoorIdentity(record, found->second->door);
    CheckOriginalDoorEntry(record, found->second->door);
  }
  return length / 12;
}

std::size_t CheckRoomDoorInventory(const VS::Content::DataStore& store,
                                   int zone,
                                   const VS::Game::EncounterRoom& room,
                                   const DoorCaseIndex& indexed) {
  CAPTURE(zone, room.index, room.asset);
  if (room.asset.empty()) {
    CHECK_FALSE(room.unavailable_reason.empty());
    return 0;
  }
  const auto* entry = store.Find(room.asset);
  REQUIRE(entry != nullptr);
  const auto file = store.ReadAll(*entry);
  REQUIRE(file.has_value());
  return CheckRawRoomDoors(*file, zone, room.index, indexed);
}
}  // namespace

TEST_CASE("Door inventory preserves directed aliases and original conditions",
          "[doors][inventory]") {
  auto catalog = TestDoorCatalog();
  auto duplicate = catalog.front().rooms.front().doors.front();
  duplicate.index = 1;
  catalog.front().rooms.front().doors.push_back(duplicate);
  const auto cases = VS::combat_lab::EnumerateDoorTests(catalog);
  REQUIRE(cases.size() == 3);
  std::set<std::string> ids;
  for (const auto& test : cases) {
    CHECK(ids.insert(test.id).second);
    CheckDoorFixture(test);
    CheckDoorMetadata(test);
  }
}

TEST_CASE("Unavailable door cases remain in inventory rather than disappearing",
          "[doors][inventory]") {
  auto catalog = TestDoorCatalog();
  SECTION("unsupported source") {
    catalog.front().rooms.front().unavailable_reason = "unsupported source";
  }
  SECTION("no supported fixture") {
    catalog.front().rooms.front().floor.assign(9,
                                               VS::Game::EncounterFloor::kPit);
  }
  SECTION("missing destination") {
    catalog.front().rooms.front().doors.front().destination_zone = 255;
  }
  const auto cases = VS::combat_lab::EnumerateDoorTests(catalog);
  REQUIRE(cases.size() == 2);
  CHECK_FALSE(cases.front().unavailable.empty());
  const DoorTrial trial(cases.front());
  CHECK(trial.Finished());
  CHECK(trial.Result().at("status") == "blocked");
}

TEST_CASE("Door acceptance waits for loading entry events and observation",
          "[doors][lifecycle]") {
  auto trial = RequestedDoor();
  auto state = DoorSource();
  state.room = 1;
  state.transition = 3;
  state.control_mode = 3;
  ObserveDoorWaiting(trial, state, false, 4);
  state.transition = 0;
  state.control_mode = 1;
  ObserveDoorWaiting(trial, state, false, 5);
  CHECK_FALSE(trial.Observe(state, true, 6));
  ObserveDoorWaiting(trial, state, true, 6 + DoorTrial::kObservationTicks - 1);
  CHECK_FALSE(trial.Observe(state, true, 6 + DoorTrial::kObservationTicks));
  CHECK(trial.Result().at("status") == "passed");
  CHECK(trial.Result().at("arrived_tick") == 4);
  trial.Timeout();
  CHECK(trial.Result().at("status") == "passed");
}

TEST_CASE("Door timeouts and unmet gates never become success-shaped results",
          "[doors][lifecycle]") {
  auto trial = RequestedDoor();
  SECTION("destination never loads") { trial.Timeout(); }
  SECTION("wrong destination") {
    auto state = DoorSource();
    state.room = 99;
    trial.Observe(state, true, 4);
    trial.Timeout();
  }
  SECTION("destination event stays active") {
    auto state = DoorSource();
    state.room = 1;
    trial.Observe(state, false, 4);
    trial.Timeout();
  }
  CHECK(trial.Finished());
  CHECK(trial.Result().at("status") != "passed");
  CHECK_FALSE(trial.Result().at("reason").get<std::string>().empty());
}

TEST_CASE("Door observation restarts when restored control is interrupted",
          "[doors][lifecycle]") {
  auto trial = RequestedDoor();
  auto state = DoorSource();
  state.room = 1;
  trial.Observe(state, true, 10);
  SECTION("entry event restarts") { trial.Observe(state, false, 20); }
  SECTION("control is taken") {
    auto busy = state;
    busy.control_mode = 3;
    trial.Observe(busy, true, 20);
  }
  SECTION("readiness is revoked") {
    auto loading = state;
    loading.ready = false;
    trial.Observe(loading, true, 20);
  }
  trial.Observe(state, true, 30);
  trial.Observe(state, true, 10 + DoorTrial::kObservationTicks);
  CHECK_FALSE(trial.Finished());
  trial.Observe(state, true, 30 + DoorTrial::kObservationTicks);
  CHECK(trial.Result().at("status") == "passed");
  CHECK(trial.Result().at("control_tick") == 30);
}

TEST_CASE("Door requests preserve busy blocked and invalid outcomes",
          "[doors][lifecycle]") {
  const auto test =
      VS::combat_lab::EnumerateDoorTests(TestDoorCatalog()).front();
  DoorTrial trial(test);
  trial.Requested({EncounterDoorRequestStatus::kBusy, "still playing"}, 1);
  CHECK_FALSE(trial.Finished());
  CHECK(trial.Observe(DoorSource(), true, 2));
  SECTION("unmet flag") {
    trial.Requested({EncounterDoorRequestStatus::kBlocked, "flag not met"}, 3);
    CHECK(trial.Result().at("status") == "blocked");
  }
  SECTION("invalid native index") {
    trial.Requested({EncounterDoorRequestStatus::kInvalid, "missing door"}, 3);
    CHECK(trial.Result().at("status") == "failed");
  }
  CHECK(trial.Finished());
}

TEST_CASE("Self-loop doors cannot pass without a real loading phase",
          "[doors][lifecycle]") {
  const auto test =
      VS::combat_lab::EnumerateDoorTests(TestDoorCatalog()).front();
  DoorTrial trial(test);
  trial.Requested({EncounterDoorRequestStatus::kAccepted, {}}, 1);
  auto pending = DoorSource();
  pending.transition = 1;
  trial.Observe(pending, false, 2);
  trial.Observe(DoorSource(), true, 1000);
  trial.Observe(DoorSource(), true, 1000 + DoorTrial::kObservationTicks);
  CHECK_FALSE(trial.Finished());
  trial.Timeout();
  CHECK(trial.Result().at("status") == "failed");
}

TEST_CASE("Every readable original directed door has an exact inventory case",
          "[.assets][disc-door-inventory]") {
  REQUIRE(VS::Content::DataStore::Open("Native\\data").has_value());
  const auto* store = VS::Content::DataStore::Get();
  const auto catalog = VS::Game::ReadEncounterCatalog(true);
  REQUIRE(catalog.has_value());
  const auto cases = VS::combat_lab::EnumerateDoorTests(*catalog);
  CHECK(catalog->size() == 255);
  CHECK(cases.size() == 1001);
  DoorCaseIndex indexed;
  for (const auto& test : cases) {
    CHECK(indexed
              .emplace(
                  std::tuple(test.setup.zone, test.setup.room, test.door.index),
                  &test)
              .second);
  }
  std::size_t original_doors = 0;
  for (const auto& zone : *catalog) {
    for (const auto& room : zone.rooms) {
      original_doors += CheckRoomDoorInventory(*store, zone.id, room, indexed);
    }
  }
  CHECK(original_doors == cases.size());
  CHECK(original_doors > 0);
}

TEST_CASE("Door dialogue automation uses confirm input without scene skipping",
          "[doors][dialogue]") {
  const auto test =
      VS::combat_lab::EnumerateDoorTests(TestDoorCatalog()).front();
  DoorTrial trial(test);
  CHECK_FALSE(trial.Observe(DoorSource(), false, 1, true));
  CHECK(trial.Input(30) == VS::Input::Circle);
  CHECK(trial.Input(31) == VS::Input::Circle);
  CHECK(trial.Input(32) == 0);
  CHECK(trial.Result().at("dialogue_confirm_pulses") == 1);
  trial.Observe(DoorSource(), false, 33, false);
  CHECK(trial.Input(60) == 0);
  trial.Requested({EncounterDoorRequestStatus::kBlocked, "gate"}, 61);
  CHECK(trial.Input(90) == 0);
}

TEST_CASE(
    "Diagnostic door requests cannot mutate ordinary or inactive sessions",
    "[doors][lifecycle]") {
  const auto result = VS::Game::RequestEncounterDoorTransition(0);
  CHECK(result.status == EncounterDoorRequestStatus::kInvalid);
  CHECK_FALSE(result.reason.empty());
}
