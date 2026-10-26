#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/ModuleData.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ModelRendering.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Room.h"
#include "Field/SegmentCollision.h"
#include "Render/ModelDraw.h"
#include "Render/ModelWorkspace.h"
}

static_assert(std::is_same_v<decltype(&Field_TraceActorAndTerrainCollision),
                             SVECTOR* (*)(const SVECTOR*, const SVECTOR*,
                                          SVECTOR*, int)>);
static_assert(
    std::is_same_v<decltype(&Field_IsPointOutsideSolidVolumes),
                   int (*)(const VECTOR*, const Field_PlatformContactPoint*,
                           size_t)>);

namespace {
struct SegmentFixture {
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  decltype(std::to_array(Actors_actorRecords)) saved_models =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Field_gimmickInstances)) saved_gimmicks =
      std::to_array(Field_gimmickInstances);
  Field_RoomRecord saved_room = Field_roomData;
  std::array<Battle_Actor, 2> actors{};
  std::array<Actors_ActorRecord, 2> models{};
  std::array<Battle_ActorStats, 2> stats{};
  std::array<u_char, 1304> saved_bounce{};

  SegmentFixture() {
    std::copy_n(Field_platformBounceFrames, saved_bounce.size(),
                saved_bounce.begin());
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    std::fill_n(Actors_actorRecords, saved_models.size(), nullptr);
    std::fill_n(Field_gimmickInstances, saved_gimmicks.size(), nullptr);
    Field_roomData = {};
    for (unsigned index = 0; index < actors.size(); ++index) {
      Battle_actors[index] = &actors[index];
      Actors_actorRecords[index] = &models[index];
      actors[index].id = static_cast<int>(index);
      actors[index].stats = &stats[index];
      stats[index].actorId = static_cast<u_char>(index);
      models[index].instance.position = {static_cast<std::int16_t>(index * 128),
                                         0, 128, 0};
      models[index].collisionRadius = 32;
      models[index].collisionHeight = 128;
    }
  }

  ~SegmentFixture() {
    std::copy(saved_bounce.begin(), saved_bounce.end(),
              Field_platformBounceFrames);
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
    std::copy(saved_models.begin(), saved_models.end(), Actors_actorRecords);
    std::copy(saved_gimmicks.begin(), saved_gimmicks.end(),
              Field_gimmickInstances);
    Field_roomData = saved_room;
  }
};

SVECTOR* TraceHorizontal(SVECTOR& output, int ignored_actor = 0) {
  const SVECTOR from{0, -64, 128, 77};
  const SVECTOR to{256, -64, 128, 88};
  return Field_TraceActorAndTerrainCollision(&from, &to, &output,
                                             ignored_actor);
}

struct ActionTraceFixture : SegmentFixture {
  static constexpr int kAction = 255;
  Battle_ActionDefinition definition = Battle_actionDefinitions[kAction];
  MATRIX bone_matrix = Actors_boneMatrixScratch;
  Render_ModelScratchpad workspace = *Render_ModelWorkspace();
  u_int previous_hit = Battle_previousHitResult;
  u_int prerequisite = Battle_hitPrerequisiteResult;
  u_int last_stat = Battle_lastActionStatValue;
  Battle_ActionState* action_state = Battle_actionState;
  std::unique_ptr<Battle_ActionState> owned_state =
      std::make_unique<Battle_ActionState>();

  ActionTraceFixture() {
    Battle_actionState = owned_state.get();
    Battle_actionDefinitions[kAction] = {};
    Battle_actionDefinitions[kAction].resetsHitResults = 1;
    Battle_actionDefinitions[kAction].hits[0].prerequisiteFunctionId = 1;
    Actors_boneMatrixScratch = {};
    for (int axis = 0; axis < 3; ++axis) {
      Actors_boneMatrixScratch.m[axis][axis] = 4096;
      models[0].boneMatrices[0].m[axis][axis] = 4096;
    }
    Core_Gte_Reset();
  }

  ~ActionTraceFixture() {
    Battle_actionState = action_state;
    Battle_actionDefinitions[kAction] = definition;
    Actors_boneMatrixScratch = bone_matrix;
    *Render_ModelWorkspace() = workspace;
    Battle_previousHitResult = previous_hit;
    Battle_hitPrerequisiteResult = prerequisite;
    Battle_lastActionStatValue = last_stat;
    Core_Gte_Reset();
  }
};

void CheckVolumePoint(const Field_PlatformContactPoint& volume,
                      const std::array<int, 3>& delta) {
  const VECTOR point{128 + delta[0], -64 + delta[1], 128 + delta[2], 0};
  const bool inside = delta[0] > -64 && delta[0] < 64 && delta[1] > -32 &&
                      delta[1] < 32 && delta[2] > -64 && delta[2] < 64;
  CHECK(Field_IsPointOutsideSolidVolumes(&point, &volume, 1) == !inside);
}
}  // namespace

TEST_CASE("Ranged segment tracing returns the first strict actor-bound sample",
          "[field][segment-collision]") {
  const SegmentFixture fixture;
  SVECTOR from{0, -64, 128, 77};
  SVECTOR to{256, -64, 128, 88};
  SVECTOR output{999, 998, 997, 996};
  CHECK(Field_TraceActorAndTerrainCollision(&from, &to, &output, 0) == &output);
  CHECK(output.vx == 104);
  CHECK(output.vy == -64);
  CHECK(output.vz == 128);
  CHECK(output.pad == 2);
  CHECK(from.pad == 77);
  CHECK(to.pad == 88);
}

TEST_CASE(
    "Segment tracing preserves miss output and original actor eligibility",
    "[field][segment-collision]") {
  SegmentFixture fixture;
  for (unsigned kind = 0; kind <= UINT8_MAX; ++kind) {
    fixture.actors[1].kind = static_cast<u_short>(kind | 0xA500);
    SVECTOR output{9, 8, 7, 6};
    const auto* hit = TraceHorizontal(output);
    if ((kind & 0xEA) == 0) {
      CHECK(hit == &output);
      CHECK(output.pad == 2);
    } else {
      CHECK(hit == nullptr);
      CHECK(output.vx == 9);
      CHECK(output.vy == 8);
      CHECK(output.vz == 7);
      CHECK(output.pad == 6);
    }
  }
}

TEST_CASE("Segment tracing respects reactions animation lock and ignored actor",
          "[field][segment-collision]") {
  SegmentFixture fixture;
  for (unsigned reaction = 0; reaction < 16; ++reaction) {
    fixture.models[1].instance.reactionState = static_cast<u_char>(reaction);
    SVECTOR output{};
    CHECK((TraceHorizontal(output) != nullptr) !=
          (reaction == 1 || reaction == 2));
  }
  fixture.models[1].instance.reactionState = 0;
  fixture.models[1].animationLocked = 1;
  SVECTOR output{};
  CHECK(TraceHorizontal(output) == nullptr);
  fixture.models[1].animationLocked = 0;
  Battle_actors[0] = nullptr;
  CHECK(TraceHorizontal(output, 1) == nullptr);
}

TEST_CASE("Segment tracing preserves Gimmick priority and source mode byte",
          "[field][segment-collision]") {
  const SegmentFixture fixture;
  Field_GimmickInstance object{};
  object.parent = 1;
  object.positionX = object.positionZ = 128;
  Field_gimmickInstances[1] = &object;
  SVECTOR output{};
  const int mode = GENERATE(0, 1, 256, -256);
  object.mode = static_cast<std::int16_t>(mode);
  if ((mode & 255) == 0) {
    CHECK(TraceHorizontal(output) == &output);
    CHECK(output.vx == 72);
    CHECK(output.pad == 2);
  } else {
    CHECK(TraceHorizontal(output) == nullptr);
  }
  object.parent = 0;
  CHECK(TraceHorizontal(output) == nullptr);
}

TEST_CASE("Segment tracing retains source direction and unsigned actor Z",
          "[field][segment-collision]") {
  SegmentFixture fixture;
  fixture.models[1].instance.position.vx = -8;
  SVECTOR output{};
  CHECK(TraceHorizontal(output) == nullptr);
  fixture.models[1].instance.position.vx = 128;
  fixture.models[1].instance.position.vz = -1;
  const SVECTOR from{0, -64, -1, 0}, to{256, -64, -1, 0};
  CHECK(Field_TraceActorAndTerrainCollision(&from, &to, &output, 0) == nullptr);
}

TEST_CASE(
    "Segment sampling uses signed floor steps and exactly thirty-two samples",
    "[field][segment-collision]") {
  SegmentFixture fixture;
  fixture.models[1].collisionRadius = 1;
  fixture.models[1].instance.position.vx = -33;
  const SVECTOR from{-1, -64, 128, 5}, to{-32, -64, 128, 6};
  SVECTOR output{7, 8, 9, 10};
  CHECK(Field_TraceActorAndTerrainCollision(&from, &to, &output, 0) == &output);
  CHECK(output.vx == -33);
  CHECK(output.pad == 2);
  fixture.models[1].instance.position.vx = -34;
  output = {7, 8, 9, 10};
  CHECK(Field_TraceActorAndTerrainCollision(&from, &to, &output, 0) == nullptr);
  CHECK(output.pad == 10);
}

TEST_CASE(
    "Solid-volume predicates retain strict extents and source coordinate "
    "widths",
    "[field][segment-collision]") {
  const Field_PlatformContactPoint volume{128, -96, 128, 0};
  for (const int x : {-65, -64, -63, 0, 63, 64, 65}) {
    for (const int y : {-33, -32, -31, 0, 31, 32, 33}) {
      for (const int z : {-65, -64, -63, 0, 63, 64, 65}) {
        CheckVolumePoint(volume, {x, y, z});
      }
    }
  }
  const Field_PlatformContactPoint negative{-1, -96, -1, 0};
  VECTOR point{-1, -64, -1, 0};
  CHECK(Field_IsPointOutsideSolidVolumes(&point, &negative, 1) == 1);
  point.vx = 65535;
  CHECK(Field_IsPointOutsideSolidVolumes(&point, &negative, 1) == 0);
  CHECK(Field_IsPointOutsideSolidVolumes(&point, nullptr, 0) == 1);
}

TEST_CASE("Segment tracing uses bounded native platform contact storage",
          "[field][segment-collision]") {
  const SegmentFixture fixture;
  Battle_actors[1] = nullptr;
  std::array<Field_MpdMovingPlatform, 5> platforms{};
  for (auto& platform : platforms) platform.isHidden = 1;
  platforms.back().isHidden = 0;
  platforms.back().positionX = platforms.back().positionZ = 128 * 4096;
  platforms.back().positionY = (Field_kPlatformBounceOffsets[0] + 96) * 4096;
  std::fill_n(Field_platformBounceFrames, platforms.size(), 0);
  Field_roomData.movingPlatforms = platforms.data();
  Field_roomData.header.movingPlatformsLength = sizeof platforms;
  SVECTOR output{};
  CHECK(TraceHorizontal(output) == &output);
  CHECK(output.vx == 72);
  CHECK(output.vy == -64);
  CHECK(output.vz == 128);
  CHECK(output.pad == 0);
  platforms.back().isHidden = 1;
  CHECK(TraceHorizontal(output) == nullptr);
}

TEST_CASE("Terrain segment hits preserve floor equality and aliased output",
          "[field][segment-collision]") {
  const SegmentFixture fixture;
  Battle_actors[1] = nullptr;
  SVECTOR from{32767, 0, 128, 77};
  const SVECTOR to{-32768, 0, 128, 88};
  CHECK(Field_TraceActorAndTerrainCollision(&from, &to, &from, 0) == &from);
  CHECK(from.vx == 30719);
  CHECK(from.vy == 0);
  CHECK(from.vz == 128);
  CHECK(from.pad == 0);
  CHECK(to.pad == 88);
}

TEST_CASE("Native segment and volume entries retain original typed bindings",
          "[field][segment-collision][redirect-binding]") {
  const SegmentFixture fixture;
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  auto* address = VS::Content::ResolveModuleAddress(
      symbols, "BATTLE/BATTLE.PRG", 0x800E6C34u);
  REQUIRE(address != nullptr);
  const auto trace =
      reinterpret_cast<SVECTOR* (*)(const SVECTOR*, const SVECTOR*, SVECTOR*,
                                    int)>(address);
  CHECK(trace == Field_TraceActorAndTerrainCollision);
  const SVECTOR from{0, -64, 128, 0}, to{256, -64, 128, 0};
  SVECTOR output{};
  CHECK(trace(&from, &to, &output, 0) == &output);
  CHECK(output.pad == 2);
  address = VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                              0x800E6BA0u);
  REQUIRE(address != nullptr);
  using VolumePredicate =
      int (*)(const VECTOR*, const Field_PlatformContactPoint*, size_t);
  const auto outside = reinterpret_cast<VolumePredicate>(address);
  CHECK(outside == Field_IsPointOutsideSolidVolumes);
  const VECTOR point{};
  CHECK(outside(&point, nullptr, 0) == 1);
}

TEST_CASE(
    "Production action resolution redirects a blocked positional projectile",
    "[field][segment-collision][battle]") {
  const ActionTraceFixture fixture;
  auto input = std::make_unique<Battle_ActionRequest>();
  auto result = std::make_unique<Battle_ActionRequest>();
  input->targetCount = 1;
  input->targets[0].targetsPosition = 1;
  const SVECTOR target{256, 0, 96, 0};
  std::memcpy(&input->targets[0].target, &target, sizeof target);
  Battle_ResolveActionTargets(ActionTraceFixture::kAction, result.get(),
                              input.get(), 1);
  CHECK(result->targetCount == 1);
  CHECK(result->targets[0].targetsPosition == 1);
  CHECK(result->targets[0].target.mp == 0x80);
  CHECK(result->animationParameter == 1);
  SVECTOR impact{};
  std::memcpy(&impact, &result->targets[0].target, sizeof impact);
  CHECK(impact.vx == 8);
  CHECK(impact.vy == 0);
}
