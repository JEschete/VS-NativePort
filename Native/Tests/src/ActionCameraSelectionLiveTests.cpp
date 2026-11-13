#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <iterator>
#include <memory>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Battle/ActionCameraSelectionLive.hpp"
#include "Content/ModuleData.h"
#include "Render/CameraViewLive.hpp"
#include "Render/ModelGeometryLive.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Actors/SkeletonLiveState.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Field/CameraLiveState.h"
#include "Field/RoomLoadingLiveState.h"
#include "Render/Camera.h"
#include "Render/UiPrimitives.h"
}

namespace {
namespace b = VS::Battle;
struct Model {
  Actors_ShpModel header{};
  std::array<Actors_ShpBone, 2> bones{};
};
struct LiveFixture {
  decltype(std::to_array(Actors_actorRecords)) saved_actors =
      std::to_array(Actors_actorRecords);
  Battle_ActionState* saved_state = Battle_actionState;
  decltype(Field_sphericalCamera) saved_camera = Field_sphericalCamera;
  decltype(Field_cameraTransition) saved_transition = Field_cameraTransition;
  decltype(Field_zoneContext.bounds) saved_bounds = Field_zoneContext.bounds;
  Render_FrameScratch saved_frame = Render_frameScratch;
  int saved_projection = Render_projectionDistance;
  MATRIX saved_bone_camera = Actors_boneMatrixScratch;
  Model model{};
  Actors_ActorRecord actor{};
  Battle_ActionState state{};
  LiveFixture() {
    std::ranges::fill(Actors_actorRecords, nullptr);
    VS::Render::RecordLiveModelExtent(&model, sizeof(model));
    model.bones[1].limbIds = 0x02;
    actor.instance.model = &model.header;
    actor.instance.boneCount = 2;
    actor.modelDataSize = sizeof(model);
    constexpr std::int16_t kOne = 4096;
    actor.scale = kOne;
    for (auto& matrix : actor.boneMatrices) {
      matrix.m[0][0] = kOne;
      matrix.m[1][1] = kOne;
      matrix.m[2][2] = kOne;
    }
    actor.boneMatrices[0].t[0] = -1;
    actor.boneMatrices[0].t[1] = -5;
    actor.boneMatrices[0].t[2] = 9;
    actor.boneMatrices[1].t[0] = 3;
    actor.boneMatrices[1].t[1] = 4;
    Actors_actorRecords[0] = &actor;
    Actors_boneMatrixScratch = {};
    Actors_boneMatrixScratch.m[0][0] = kOne;
    Actors_boneMatrixScratch.m[1][1] = kOne;
    Actors_boneMatrixScratch.m[2][2] = kOne;
    Render_frameScratch.viewMatrix = Actors_boneMatrixScratch;
    Render_frameScratch.viewMatrix.t[2] = 1024;
    Render_frameScratch.camera = {};
    Field_cameraTransition = nullptr;
    Field_zoneContext.bounds = {-100, -100, 100, 100};
    Battle_actionState = &state;
    state.queueIndex = 3;
    state.queue[3].focusPosition = {-4, -2, 2, 71};
  }
  LiveFixture(const LiveFixture&) = delete;
  LiveFixture& operator=(const LiveFixture&) = delete;
  ~LiveFixture() {
    std::ranges::copy(saved_actors, std::begin(Actors_actorRecords));
    Battle_actionState = saved_state;
    Field_sphericalCamera = saved_camera;
    Field_cameraTransition = saved_transition;
    Field_zoneContext.bounds = saved_bounds;
    Render_frameScratch = saved_frame;
    VS::Render::SetLiveCameraProjectionDistance(saved_projection);
    Actors_boneMatrixScratch = saved_bone_camera;
    VS::Render::ForgetLiveModelExtent(&model);
  }
};
using Point = std::array<std::int16_t, 4>;
static_assert(std::is_same_v<decltype(&b::LiveActionTargetLimbPosition),
                             void (*)(int, int, SVECTOR*)>);
static_assert(
    std::is_same_v<decltype(&b::LiveActionTargetDistance), int (*)()>);
static_assert(
    std::is_same_v<decltype(&b::LiveActionCameraMidpoint), void (*)(SVECTOR*)>);
static_assert(std::is_same_v<decltype(&b::LiveStepCameraTowardActionMidpoint),
                             void (*)(int)>);
static_assert(
    std::is_same_v<decltype(&b::LiveUpdateInitialTargetSelection), void (*)()>);
static_assert(
    std::is_same_v<decltype(&b::LiveUpdateActionAreaSelection), void (*)()>);
static_assert(
    std::is_same_v<decltype(&b::LiveUpdateFinalTargetSelection), void (*)()>);
static_assert(
    std::is_same_v<decltype(&b::LiveUpdateActionCameraZoom), void (*)()>);

TEST_CASE("F00179 production queries loaded limb and root without writing pad",
          "[camera-selection][F00179][redirect-binding]") {
  auto fixture = std::make_unique<LiveFixture>();
  SVECTOR point{99, 98, 97, 96};
  b::LiveActionTargetLimbPosition(0, 1, &point);
  CHECK(std::bit_cast<Point>(point) == Point{3, 4, 0, 96});
  b::LiveActionTargetLimbPosition(0, -1, &point);
  CHECK(std::bit_cast<Point>(point) == Point{-1, -5, 9, 96});
  b::LiveActionTargetLimbPosition(0, -1, &fixture->actor.instance.position);
  CHECK(fixture->actor.instance.position.vx == -1);
}
TEST_CASE(
    "F00180 production distance uses queue three and real approximate roots",
    "[camera-selection][F00180][redirect-binding]") {
  auto fixture = std::make_unique<LiveFixture>();
  auto& queue = fixture->state.queue[3];
  CHECK(b::LiveActionTargetDistance() == 0);
  queue.targetCount = 65535;
  queue.targets[0].targetsPosition = -1;
  queue.targets[0].target =
      std::bit_cast<decltype(queue.targets[0].target)>(Point{2, -1, 9, 71});
  CHECK(b::LiveActionTargetDistance() == 5);
  queue.targets[0].targetsPosition = 0;
  queue.targets[0].target.targetActor = 0;
  queue.targets[0].target.targetLimb = -1;
  CHECK(b::LiveActionTargetDistance() == 0);
  fixture->actor.boneMatrices[0].t[0] = -2426;
  fixture->actor.boneMatrices[0].t[1] = 7487;
  fixture->actor.boneMatrices[0].t[2] = 1009;
  queue.targets[0].targetsPosition = 1;
  queue.targets[0].target = std::bit_cast<decltype(queue.targets[0].target)>(
      Point{-892, 4966, -6156, 4966});
  CHECK(b::LiveActionTargetDistance() == 7731);
}
TEST_CASE(
    "F00181 production alias keeps focus padding and source-relative rounding",
    "[camera-selection][F00181][redirect-binding]") {
  auto fixture = std::make_unique<LiveFixture>();
  auto& focus = fixture->state.queue[3].focusPosition;
  b::LiveActionCameraMidpoint(&focus);
  CHECK(std::bit_cast<Point>(focus) == Point{-2, -4, 6, 71});
}
TEST_CASE("F00182 production dry step still queries the live skeleton",
          "[camera-selection][F00182][redirect-binding]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->state.queue[3].targetCount = 1;
  fixture->state.queue[3].targets[0].target.targetLimb = -1;
  b::LiveStepCameraTowardActionMidpoint(0);
  CHECK(fixture->state.queue[3].targetCount == 1);
  CHECK(fixture->state.queueIndex == 3);
}
TEST_CASE("F00183 production enemy wait has no actor record dependency",
          "[camera-selection][F00183][redirect-binding]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->state.isPlayerAction = -1;
  fixture->state.selection.primaryTarget.target.targetActor = 255;
  fixture->state.selectionFrames = static_cast<char>(255);
  fixture->state.selectedPreview = 77;
  b::LiveUpdateInitialTargetSelection();
  CHECK(static_cast<unsigned char>(fixture->state.selectionFrames) == 0);
  CHECK(fixture->state.selectedPreview == 77);
}
TEST_CASE("F00184 production area wait wraps unsigned frames before readiness",
          "[camera-selection][F00184][redirect-binding]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->state.isPlayerAction = 0x10000;
  fixture->state.selection.primaryTarget.target.targetActor = 255;
  fixture->state.selectionFrames = static_cast<char>(255);
  fixture->state.areaCenter = {1, 2, 3, 4};
  b::LiveUpdateActionAreaSelection();
  CHECK(static_cast<unsigned char>(fixture->state.selectionFrames) == 0);
  CHECK(std::bit_cast<Point>(fixture->state.areaCenter) == Point{1, 2, 3, 4});
}
TEST_CASE(
    "F00185 production enemy wait never consults unused cancel definition",
    "[camera-selection][F00185][redirect-binding]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->state.isPlayerAction = 1;
  fixture->state.selection.primaryTarget.target.targetActor = 255;
  fixture->state.selection.actionId = 65535;
  fixture->state.selectionFrames = static_cast<char>(255);
  fixture->state.selectedPreview = 255;
  b::LiveUpdateFinalTargetSelection();
  CHECK(static_cast<unsigned char>(fixture->state.selectionFrames) == 0);
  CHECK(fixture->state.selectedPreview == 255);
}
TEST_CASE(
    "F00186 production inactive diagnostics do not read missing action state",
    "[camera-selection][F00186][redirect-binding]") {
  auto fixture = std::make_unique<LiveFixture>();
  Battle_actionState = nullptr;
  for (int mode : {0, 2, 4, -1}) {
    Field_sphericalCamera.delta.mode = mode;
    b::LiveUpdateActionCameraZoom();
    CHECK(Field_sphericalCamera.delta.mode == mode);
  }
}
TEST_CASE(
    "F00186 production zoom uses the real Field tracking and Render setter",
    "[camera-selection][F00186][F00182][redirect-binding]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->state.cameraZoomState = 1;
  Field_sphericalCamera = {};
  Field_sphericalCamera.delta.mode = 1;
  Field_sphericalCamera.delta.yaw = 1;
  Field_sphericalCamera.values.distance = 1600;
  VS::Render::SetLiveCameraProjectionDistance(704);
  b::LiveUpdateActionCameraZoom();
  CHECK(Render_projectionDistance == 768);
  CHECK(Field_sphericalCamera.values.distance == 1536);
  CHECK(Field_sphericalCamera.delta.yaw == 1);
  CHECK(Field_sphericalCamera.speed > 0);
  CHECK((Render_frameScratch.camera.lookAt.vx != 0 ||
         Render_frameScratch.camera.lookAt.vz != 0));
  CHECK(fixture->state.queueIndex == 3);
}
TEST_CASE("F00179 through F00186 module entries retain their actual call ABI",
          "[camera-selection][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const std::array<std::pair<std::uint32_t, void*>, 8> bindings{{
      {0x80077F14, reinterpret_cast<void*>(&b::LiveActionTargetLimbPosition)},
      {0x80077F70, reinterpret_cast<void*>(&b::LiveActionTargetDistance)},
      {0x800780A8, reinterpret_cast<void*>(&b::LiveActionCameraMidpoint)},
      {0x8007820C,
       reinterpret_cast<void*>(&b::LiveStepCameraTowardActionMidpoint)},
      {0x80078248,
       reinterpret_cast<void*>(&b::LiveUpdateInitialTargetSelection)},
      {0x800782E4, reinterpret_cast<void*>(&b::LiveUpdateActionAreaSelection)},
      {0x80078364, reinterpret_cast<void*>(&b::LiveUpdateFinalTargetSelection)},
      {0x800784AC, reinterpret_cast<void*>(&b::LiveUpdateActionCameraZoom)},
  }};
  for (const auto& [address, entry] : bindings) {
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address) == entry);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address + 4) == nullptr);
  }
}
TEST_CASE("Camera selection relocation rejects interior entrypoints atomically",
          "[camera-selection][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  std::array<std::uint32_t, 4> addresses{0x80078248, 0x800782E4, 0x80078364,
                                         0x800784AC};
  std::array<void*, 4> destination{};
  const std::array<void*, 4> expected{
      reinterpret_cast<void*>(&b::LiveUpdateInitialTargetSelection),
      reinterpret_cast<void*>(&b::LiveUpdateActionAreaSelection),
      reinterpret_cast<void*>(&b::LiveUpdateFinalTargetSelection),
      reinterpret_cast<void*>(&b::LiveUpdateActionCameraZoom)};
  const auto image =
      std::bit_cast<std::array<unsigned char, sizeof(addresses)>>(addresses);
  const Content_ModulePointerTable table{"BATTLE/BATTLE.PRG", 0, 4,
                                         destination.data(),  4, nullptr};
  REQUIRE(VS::Content::RelocateModulePointerTable(table, image, symbols));
  CHECK(destination == expected);
  addresses[3] += 4;
  const auto invalid =
      std::bit_cast<std::array<unsigned char, sizeof(addresses)>>(addresses);
  CHECK_FALSE(VS::Content::RelocateModulePointerTable(table, invalid, symbols));
  CHECK(destination == expected);
}
}  // namespace
