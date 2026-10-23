#include <array>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Effects/Runtime.h"
#include "Effects/ScriptVm.h"
}

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

TEST_CASE("Effect register instructions consume the index before the halfword",
          "[effects][script]") {
  const auto index = GENERATE(0U, 1U, 2U, 3U);
  const auto value = GENERATE(0U, 1U, 2U, 255U, 0x101U, 0x202U, 0x3FFU);
  std::array<char, 3> script{static_cast<char>(index), static_cast<char>(value),
                             static_cast<char>(value >> 8)};
  Effects_EffectInstance instance{};
  instance.script = script.data();
  for (unsigned reg = 0; reg < 4; ++reg) {
    instance.registers[reg] = static_cast<char>(0x40 + reg);
  }
  REQUIRE(Effects_SetEffectScriptRegister(&instance) == 1);
  CHECK(instance.scriptOffset == script.size());
  for (unsigned reg = 0; reg < 4; ++reg) {
    CHECK(static_cast<u_char>(instance.registers[reg]) ==
          (reg == index ? static_cast<u_char>(value) : 0x40 + reg));
  }
}

TEST_CASE(
    "Effect register branches consume comparison operands in source order",
    "[effects][script]") {
  const auto index = GENERATE(0U, 1U, 2U, 3U);
  const auto value = GENERATE(0U, 1U, 2U, 255U, 0x101U);
  const bool equal = GENERATE(false, true);
  std::array<char, 5> script{static_cast<char>(index), static_cast<char>(value),
                             static_cast<char>(value >> 8), 0x34, 0x12};
  Effects_EffectInstance instance{};
  instance.script = script.data();
  instance.registers[index] = static_cast<char>(equal ? value : value ^ 1U);
  REQUIRE(Effects_BranchIfEffectRegisterEqual(&instance) == 1);
  CHECK(instance.scriptOffset ==
        (equal && value <= 255U ? 0x1234 : script.size()));
}
