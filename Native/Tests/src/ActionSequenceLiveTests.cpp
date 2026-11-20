#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Action.h"
#include "Battle/ActionExecution.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionSequenceLive.hpp"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Damage.h"
#include "Content/ModuleData.hpp"
#include "Core/Gte.h"
#include "Effects/Runtime.hpp"
#include "Field/CameraLiveState.hpp"
#include "Menu/TextBox.hpp"
#include "ModelLoading.h"
#include "Render/Camera.hpp"
#include "Render/CameraViewLive.hpp"

namespace {
namespace b = VS::Battle;
struct LiveFixture {
  decltype(std::to_array(Actors_actorRecords)) saved_records =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  Battle_ActionState* saved_state = Battle_actionState;
  decltype(Battle_Ai_roomAiState) saved_room = Battle_Ai_roomAiState;
  decltype(VS::Effects::secondaryInstances) saved_trails =
      VS::Effects::secondaryInstances;
  decltype(VS::Menu::sceneNotificationBuffer) saved_notification =
      VS::Menu::sceneNotificationBuffer;
  decltype(VS::Field::sphericalCamera) saved_camera =
      VS::Field::sphericalCamera;
  Content_ActorAssetRequest saved_request = Content_legacyActorAssetRequests[0];
  std::array<unsigned, 2> saved_effects{VS::Effects::primaryPhase,
                                        VS::Effects::pendingTracks};
  std::array<int, 2> saved_tracks{VS::Effects::trackCount,
                                  VS::Effects::startedTrackCount};
  decltype(VS::Effects::actionVariant) saved_variant =
      VS::Effects::actionVariant;
  int saved_busy = VS::Effects::effectBusyFlags;
  int saved_projection = VS::Render::projectionDistance;
  int saved_distance_alias = Battle_settledCameraDistance;
  unsigned saved_phase = Battle_actionFlowMode;
  Battle_ActionState state{};
  Battle_Ai_RoomAiState room{};
  Actors_ActorRecord record{};
  Battle_Actor actor{};
  LiveFixture() {
    std::ranges::fill(Actors_actorRecords, nullptr);
    std::ranges::fill(Battle_actors, nullptr);
    Battle_actionState = &state;
    Battle_Ai_roomAiState = &room;
    VS::Effects::secondaryInstances = nullptr;
    VS::Menu::sceneNotificationBuffer = nullptr;
  }
  LiveFixture(const LiveFixture&) = delete;
  LiveFixture& operator=(const LiveFixture&) = delete;
  ~LiveFixture() {
    std::ranges::copy(saved_records, std::begin(Actors_actorRecords));
    std::ranges::copy(saved_actors, std::begin(Battle_actors));
    Battle_actionState = saved_state;
    Battle_Ai_roomAiState = saved_room;
    VS::Effects::secondaryInstances = saved_trails;
    VS::Menu::sceneNotificationBuffer = saved_notification;
    VS::Field::sphericalCamera = saved_camera;
    Content_legacyActorAssetRequests[0] = saved_request;
    VS::Effects::primaryPhase = saved_effects[0];
    VS::Effects::pendingTracks = saved_effects[1];
    VS::Effects::trackCount = saved_tracks[0];
    VS::Effects::startedTrackCount = saved_tracks[1];
    VS::Effects::actionVariant = saved_variant;
    VS::Effects::effectBusyFlags = saved_busy;
    Battle_actionFlowMode = saved_phase;
    Battle_settledCameraDistance = saved_distance_alias;
    VS::Render::projectionDistance = saved_projection;
  }
};
struct ProjectionReset {
  ProjectionReset() = default;
  ProjectionReset(const ProjectionReset&) = delete;
  ProjectionReset& operator=(const ProjectionReset&) = delete;
  ~ProjectionReset() { Core_Gte_Reset(); }
};

TEST_CASE("Action sequence bindings retain five original VOID0 module ABIs",
          "[action-sequence][redirect-binding]") {
  static_assert(
      std::is_same_v<decltype(&b::WaitForLiveActionEffectReady), void (*)()>);
  static_assert(
      std::is_same_v<decltype(&b::StartLiveReadyActionAnimation), void (*)()>);
  static_assert(
      std::is_same_v<decltype(&b::WaitForLiveActionResolution), void (*)()>);
  static_assert(
      std::is_same_v<decltype(&b::WaitForLiveTargetRecovery), void (*)()>);
  static_assert(
      std::is_same_v<decltype(&b::FinishLiveActionSequence), void (*)()>);
  const auto symbols = VS::Content::ModuleSymbols();
  const std::array<std::uint32_t, 5> addresses{
      0x80078748, 0x800787F0, 0x80079050, 0x800790BC, 0x800793C0};
  const std::array entries{
      &b::WaitForLiveActionEffectReady, &b::StartLiveReadyActionAnimation,
      &b::WaitForLiveActionResolution, &b::WaitForLiveTargetRecovery,
      &b::FinishLiveActionSequence};
  std::array<void*, 5> relocated{};
  for (std::size_t i = 0; i < addresses.size(); ++i) {
    std::array<unsigned char, 4> image{};
    for (std::size_t byte = 0; byte < image.size(); ++byte) {
      image.at(byte) =
          static_cast<unsigned char>(addresses.at(i) >> (byte * 8));
    }
    VS::Content::ModulePointerTable table{"BATTLE/BATTLE.PRG", 0, 1,
                                          &relocated.at(i),    1, nullptr};
    REQUIRE(VS::Content::RelocateModulePointerTable(table, image, symbols));
    CHECK(relocated.at(i) == reinterpret_cast<void*>(entries.at(i)));
  }
}
TEST_CASE(
    "F00187 production actor assets and effect gates advance the owned phase",
    "[action-sequence][F00187][action-presentation][actors-state]") {
  auto fixture = std::make_unique<LiveFixture>();
  Actors_actorRecords[15] = &fixture->record;
  fixture->record.instance.slot = 15;
  fixture->record.pose.holdingLastFrame = -1;
  fixture->record.pose.reachedEnd = -1;
  fixture->state.queueIndex = 0xFFFFFFFF;
  fixture->state.queue[3].primaryTarget.target.targetActor = 15;
  Content_legacyActorAssetRequests[0].assetKind = 0;
  VS::Effects::primaryPhase = 4;
  VS::Effects::trackCount = 1;
  VS::Effects::startedTrackCount = 0;
  VS::Effects::pendingTracks = 1;
  VS::Effects::actionVariant = 19;
  Battle_actionFlowMode = 5;
  b::WaitForLiveActionEffectReady();
  CHECK(Battle_actionFlowMode == 6);
  CHECK(VS::Effects::actionVariant == 0);
  CHECK(fixture->state.queueIndex == 0xFFFFFFFF);
}
TEST_CASE("F00188 production busy bit uses no missing action owner",
          "[action-sequence][F00188]") {
  auto fixture = std::make_unique<LiveFixture>();
  Battle_actionState = nullptr;
  VS::Effects::effectBusyFlags = -1;
  Battle_actionFlowMode = 19;
  b::StartLiveReadyActionAnimation();
  CHECK(Battle_actionState == nullptr);
  CHECK(Battle_actionFlowMode == 19);
}
TEST_CASE("F00192 production planning release and trail phase use real helpers",
          "[action-sequence][F00192][ai-completed-plan][action-phases]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->state.queueIndex = 0xFFFFFFFE;
  fixture->state.queue[2].primaryTarget.targetsPosition = 1;
  fixture->room.readinessLevel = 19;
  fixture->actor.commandState = 0x80;
  fixture->actor.commandOpcode = 0x18;
  Battle_actors[15] = &fixture->actor;
  b::WaitForLiveActionResolution();
  CHECK(fixture->room.readinessLevel == 0);
  CHECK(fixture->actor.commandState == 8);
  CHECK(Battle_actionFlowMode == 10);
  CHECK(fixture->state.queueIndex == 0xFFFFFFFE);
}
TEST_CASE("F00193 production255 targets use real animation readiness",
          "[action-sequence][F00193][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->state.chainStep = 1;
  fixture->state.queue[0].targetCount = 255;
  for (auto& target : fixture->state.queue[0].targets) {
    target.target.targetActor = 15;
  }
  fixture->record.instance.actionAnimationState = 1;
  Actors_actorRecords[15] = &fixture->record;
  Battle_actionFlowMode = 10;
  b::WaitForLiveTargetRecovery();
  CHECK(Battle_actionFlowMode == 10);
  CHECK(fixture->state.queue[0].targetCount == 255);
}
TEST_CASE(
    "F00195 production restoration uses the native renderer and signed "
    "preferred distance",
    "[action-sequence][F00195][camera-view]") {
  const ProjectionReset reset;
  auto fixture = std::make_unique<LiveFixture>();
  VS::Field::sphericalCamera.motion.zoom_mode = 3;
  VS::Field::sphericalCamera.values.distance = -32600;
  fixture->state.preferredCameraDistance = -32768;
  Battle_settledCameraDistance = 12345;
  VS::Render::SetLiveCameraProjectionDistance(577);
  b::FinishLiveActionSequence();
  CHECK(VS::Render::projectionDistance == 513);
  CHECK(VS::Field::sphericalCamera.values.distance == -32768);
  CHECK(Battle_settledCameraDistance == 12345);
  CHECK(fixture->state.chainStep == 0);
}
TEST_CASE(
    "F00195 production early projection gate leaves an unused owner absent",
    "[action-sequence][F00195]") {
  const ProjectionReset reset;
  auto fixture = std::make_unique<LiveFixture>();
  Battle_actionState = nullptr;
  VS::Field::sphericalCamera.motion.zoom_mode = 0;
  VS::Render::SetLiveCameraProjectionDistance(511);
  b::FinishLiveActionSequence();
  CHECK(Battle_actionState == nullptr);
  CHECK(VS::Render::projectionDistance == 511);
}
}  // namespace
