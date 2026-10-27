#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Event/ScriptField.h"
#include "State/RoomMemory.h"
}

namespace {
struct RoomActorCacheFixture {
  decltype(std::to_array(State_recentRoomEnemyStates)) saved =
      std::to_array(State_recentRoomEnemyStates);
  int saved_slot = State_currentRoomStateSlot;

  RoomActorCacheFixture() {
    constexpr std::array<decltype(State_recentRoomEnemyStates[0].mapId), 4>
        kRooms{7, 7, 8, 7};
    constexpr std::array<decltype(State_recentRoomEnemyStates[0].zoneId), 4>
        kZones{31, 32, 31, 31};
    for (std::size_t index = 0; index < kRooms.size(); ++index) {
      auto& record = State_recentRoomEnemyStates[index];
      record.mapId = kRooms[index];
      record.zoneId = kZones[index];
      std::memset(record.actorStates, 0x5A, sizeof(record.actorStates));
    }
    State_currentRoomStateSlot = 1;
  }

  ~RoomActorCacheFixture() {
    std::copy(saved.begin(), saved.end(), State_recentRoomEnemyStates);
    State_currentRoomStateSlot = saved_slot;
  }
};

void CheckRoomActorCache(std::size_t index, int room, int zone,
                         std::byte expected) {
  const auto& record = State_recentRoomEnemyStates[index];
  CHECK(record.mapId == room);
  CHECK(record.zoneId == zone);
  const auto bytes = std::as_bytes(std::span(record.actorStates));
  CHECK(std::ranges::all_of(
      bytes, [expected](std::byte value) { return value == expected; }));
}
}  // namespace

extern "C" int Tests_EventScriptNativeBranches(int skip);
extern "C" int Tests_EventScriptBranchHandlers(void);
extern "C" int Tests_NativeSessionTransitions(void);
extern "C" int Tests_AudioModesWithoutSavedMusic(void);
extern "C" int Tests_RoomEffectCommandLayout(void);
extern "C" int Tests_RoomLightingSamples(void);

TEST_CASE("Room lighting samples both layers and interpolates vertex colors",
          "[render][lighting]") {
  REQUIRE(Tests_RoomLightingSamples() == 1);
}

TEST_CASE(
    "Room effect transforms and velocities read fixed-width disk operands",
    "[event][room-effect]") {
  REQUIRE(Tests_RoomEffectCommandLayout() == 1);
}

TEST_CASE("Audio voice modes support playback without a saved music layer",
          "[event][audio]") {
  REQUIRE(Tests_AudioModesWithoutSavedMusic() == 1);
}

TEST_CASE(
    "Native session transitions re-enter title and battle without "
    "stack-pointer emulation",
    "[boot][lifecycle]") {
  REQUIRE(Tests_NativeSessionTransitions() == 1);
}

TEST_CASE("Event interpreter preserves native branch addresses",
          "[event][script]") {
  SECTION("normal dispatch") {
    REQUIRE(Tests_EventScriptNativeBranches(0) == 1);
  }
  SECTION("skip dispatch") { REQUIRE(Tests_EventScriptNativeBranches(1) == 1); }
}

TEST_CASE("Event branch handlers return complete native addresses",
          "[event][script]") {
  REQUIRE(Tests_EventScriptBranchHandlers() == 1);
}

TEST_CASE("Room cache opcode clears matching room and zone without loading",
          "[event][room-cache]") {
  RoomActorCacheFixture fixture;
  std::array<u_char, 3> command{0, 31, 7};
  CHECK(Event_ScriptClearRoomActorCache(command.data(), 2) == 0);
  CheckRoomActorCache(0, 7, 31, std::byte{0});
  CheckRoomActorCache(1, 7, 32, std::byte{0x5A});
  CheckRoomActorCache(2, 8, 31, std::byte{0x5A});
  CheckRoomActorCache(3, 7, 31, std::byte{0});
  CHECK(State_currentRoomStateSlot == 1);
  CHECK(Event_ScriptClearRoomActorCache(command.data(), 0) == 0);
  command = {0, 255, 255};
  CHECK(Event_ScriptClearRoomActorCache(command.data(), 3) == 0);
  CheckRoomActorCache(0, 7, 31, std::byte{0});
  CheckRoomActorCache(1, 7, 32, std::byte{0x5A});
}
