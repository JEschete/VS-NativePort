#include <catch2/catch_test_macros.hpp>

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
