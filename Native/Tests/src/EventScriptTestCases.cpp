#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Event/ScriptControlOpcodes.hpp"
#include "Legacy/Psx.h"
#include "State/RoomMemory.hpp"

namespace {
struct RoomActorCacheFixture {
  decltype(std::to_array(VS::State::recentRoomEnemyStates)) saved =
      std::to_array(VS::State::recentRoomEnemyStates);
  int saved_slot = VS::State::currentRoomStateSlot;

  RoomActorCacheFixture() {
    constexpr std::array<decltype(VS::State::recentRoomEnemyStates[0].map_id),
                         4>
        kRooms{7, 7, 8, 7};
    constexpr std::array<decltype(VS::State::recentRoomEnemyStates[0].zone_id),
                         4>
        kZones{31, 32, 31, 31};
    for (std::size_t index = 0; index < kRooms.size(); ++index) {
      auto& record = VS::State::recentRoomEnemyStates[index];
      record.map_id = kRooms[index];
      record.zone_id = kZones[index];
      std::memset(record.actor_states, 0x5A, sizeof(record.actor_states));
    }
    VS::State::currentRoomStateSlot = 1;
  }

  ~RoomActorCacheFixture() {
    std::copy(saved.begin(), saved.end(), VS::State::recentRoomEnemyStates);
    VS::State::currentRoomStateSlot = saved_slot;
  }
};

void CheckRoomActorCache(std::size_t index, int room, int zone,
                         std::byte expected) {
  const auto& record = VS::State::recentRoomEnemyStates[index];
  CHECK(record.map_id == room);
  CHECK(record.zone_id == zone);
  const auto bytes = std::as_bytes(std::span(record.actor_states));
  CHECK(std::ranges::all_of(
      bytes, [expected](std::byte value) { return value == expected; }));
}
}  // namespace

int Tests_EventScriptNativeBranches(int skip);
int Tests_EventScriptBranchHandlers(void);
int Tests_NativeSessionTransitions(void);
int Tests_AudioModesWithoutSavedMusic(void);
int Tests_RoomLightingSamples(void);

TEST_CASE("Room lighting samples both layers and interpolates vertex colors",
          "[render][lighting]") {
  REQUIRE(Tests_RoomLightingSamples() == 1);
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
  CHECK(VS::Event::ScriptClearRoomActorCacheOpcode(command.data(), 2) == 0);
  CheckRoomActorCache(0, 7, 31, std::byte{0});
  CheckRoomActorCache(1, 7, 32, std::byte{0x5A});
  CheckRoomActorCache(2, 8, 31, std::byte{0x5A});
  CheckRoomActorCache(3, 7, 31, std::byte{0});
  CHECK(VS::State::currentRoomStateSlot == 1);
  CHECK(VS::Event::ScriptClearRoomActorCacheOpcode(command.data(), 0) == 0);
  command = {0, 255, 255};
  CHECK(VS::Event::ScriptClearRoomActorCacheOpcode(command.data(), 3) == 0);
  CheckRoomActorCache(0, 7, 31, std::byte{0});
  CheckRoomActorCache(1, 7, 32, std::byte{0x5A});
}
