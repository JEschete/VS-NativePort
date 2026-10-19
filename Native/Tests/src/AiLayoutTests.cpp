#include <catch2/catch_test_macros.hpp>

extern "C" int Tests_LayoutAssertsLinked(void);
extern "C" int Tests_AiViewsRoundTrip(void);
extern "C" int Tests_EffectTimelineTrackStride(void);
extern "C" int Tests_RoomAiMapStorage(void);
extern "C" int Tests_GimmickViewsRoundTrip(void);
extern "C" int Tests_CollisionHeightPatterns(void);
extern "C" int Tests_ActorConditionSnapshot(void);
extern "C" int Tests_AiFrameCache(void);
extern "C" int Tests_AiFrameStages(void);
extern "C" int Tests_TargetSphereViews(void);
extern "C" int Tests_AiMovementSetup(void);
extern "C" int Tests_AiRelations(void);
extern "C" int Tests_AiMovementDispatch(void);

TEST_CASE(
    "AI movement dispatch invokes the configured native decision callback",
    "[ai][frame]") {
  REQUIRE(Tests_AiMovementDispatch() == 1);
}

TEST_CASE(
    "AI relationships initialize sparse actors reciprocally and preserve saved "
    "fields",
    "[ai][relations]") {
  REQUIRE(Tests_AiRelations() == 1);
}

TEST_CASE(
    "AI decision timers and locomotion setup retain bounded state transitions",
    "[ai][frame]") {
  REQUIRE(Tests_AiMovementSetup() == 1);
}

TEST_CASE(
    "Targeting views preserve native lists, labels, and wireframe storage",
    "[targeting][layout]") {
  REQUIRE(Tests_TargetSphereViews() == 1);
}

TEST_CASE("AI frame stages gate on navigation and rotate perception targets",
          "[ai][frame]") {
  REQUIRE(Tests_AiFrameStages() == 1);
}

TEST_CASE("AI frame context keeps native pointers out of legacy scratch",
          "[ai][frame]") {
  REQUIRE(Tests_AiFrameCache() == 1);
}

TEST_CASE("AI condition snapshots populate the linked actor IDs before scoring",
          "[ai][condition]") {
  REQUIRE(Tests_ActorConditionSnapshot() == 1);
}

TEST_CASE("Collision height patterns retain layer offsets and surface flags",
          "[collision][room]") {
  REQUIRE(Tests_CollisionHeightPatterns() == 1);
}

TEST_CASE("Gimmick motion views retain native model and reaction fields",
          "[gimmick][layout]") {
  REQUIRE(Tests_GimmickViewsRoundTrip() == 1);
}

TEST_CASE("Room AI maps preserve native storage and collision metadata",
          "[ai][room]") {
  REQUIRE(Tests_RoomAiMapStorage() == 1);
}

TEST_CASE("AI runtime views share native pointers and full state storage",
          "[ai][layout]") {
  REQUIRE(Tests_LayoutAssertsLinked() == 1);
  REQUIRE(Tests_AiViewsRoundTrip() == 1);
}

TEST_CASE("Effect timeline indexes native tracks rather than PS1-sized records",
          "[effects][layout]") {
  REQUIRE(Tests_EffectTimelineTrackStride() == 1);
}
