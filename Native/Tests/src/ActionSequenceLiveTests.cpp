#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorRecord.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionExecution.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionSequenceLive.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiState.hpp"
#include "Battle/Damage.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Gte.h"
#include "Effects/Runtime.hpp"
#include "Field/CameraLiveState.hpp"
#include "Menu/TextBox.hpp"
#include "Render/Camera.hpp"
#include "Render/CameraViewLive.hpp"

using VS::Battle::ActionState;

namespace {
namespace b = VS::Battle;
struct LiveFixture {
  decltype(VS::Actors::actorRecords) saved_records = VS::Actors::actorRecords;
  decltype(VS::Battle::actors) saved_actors = VS::Battle::actors;
  ActionState* saved_state = VS::Battle::actionState;
  decltype(VS::Battle::Ai::roomAiState) saved_room =
      VS::Battle::Ai::roomAiState;
  decltype(VS::Effects::secondaryInstances) saved_trails =
      VS::Effects::secondaryInstances;
  decltype(VS::Menu::sceneNotificationBuffer) saved_notification =
      VS::Menu::sceneNotificationBuffer;
  decltype(VS::Field::sphericalCamera) saved_camera =
      VS::Field::sphericalCamera;
  VS::Content::LegacyActorAssetRequest saved_request =
      VS::Content::legacyActorAssetRequests[0];
  std::array<unsigned, 2> saved_effects{VS::Effects::primaryPhase,
                                        VS::Effects::pendingTracks};
  std::array<int, 2> saved_tracks{VS::Effects::trackCount,
                                  VS::Effects::startedTrackCount};
  decltype(VS::Effects::actionVariant) saved_variant =
      VS::Effects::actionVariant;
  int saved_busy = VS::Effects::effectBusyFlags;
  int saved_projection = VS::Render::projectionDistance;
  int saved_distance_alias = VS::Battle::settledCameraDistance;
  unsigned saved_phase = VS::Battle::actionFlowMode;
  ActionState state{};
  VS::Battle::Ai::RoomAiState room{};
  VS::Actors::ActorRecord record{};
  VS::Battle::Actor actor{};
  LiveFixture() {
    std::ranges::fill(VS::Actors::actorRecords, nullptr);
    std::ranges::fill(VS::Battle::actors, nullptr);
    VS::Battle::actionState = &state;
    VS::Battle::Ai::roomAiState = &room;
    VS::Effects::secondaryInstances = nullptr;
    VS::Menu::sceneNotificationBuffer = nullptr;
  }
  LiveFixture(const LiveFixture&) = delete;
  LiveFixture& operator=(const LiveFixture&) = delete;
  ~LiveFixture() {
    std::ranges::copy(saved_records, std::begin(VS::Actors::actorRecords));
    std::ranges::copy(saved_actors, std::begin(VS::Battle::actors));
    VS::Battle::actionState = saved_state;
    VS::Battle::Ai::roomAiState = saved_room;
    VS::Effects::secondaryInstances = saved_trails;
    VS::Menu::sceneNotificationBuffer = saved_notification;
    VS::Field::sphericalCamera = saved_camera;
    VS::Content::legacyActorAssetRequests[0] = saved_request;
    VS::Effects::primaryPhase = saved_effects[0];
    VS::Effects::pendingTracks = saved_effects[1];
    VS::Effects::trackCount = saved_tracks[0];
    VS::Effects::startedTrackCount = saved_tracks[1];
    VS::Effects::actionVariant = saved_variant;
    VS::Effects::effectBusyFlags = saved_busy;
    VS::Battle::actionFlowMode = saved_phase;
    VS::Battle::settledCameraDistance = saved_distance_alias;
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
  VS::Actors::actorRecords[15] = &fixture->record;
  fixture->record.instance.slot = 15;
  fixture->record.pose.holding_last_frame = -1;
  fixture->record.pose.reached_end = -1;
  fixture->state.queue_index = 0xFFFFFFFF;
  fixture->state.queue[3].primary_target.target.target_actor = 15;
  VS::Content::legacyActorAssetRequests[0].asset_kind = 0;
  VS::Effects::primaryPhase = 4;
  VS::Effects::trackCount = 1;
  VS::Effects::startedTrackCount = 0;
  VS::Effects::pendingTracks = 1;
  VS::Effects::actionVariant = 19;
  VS::Battle::actionFlowMode = 5;
  b::WaitForLiveActionEffectReady();
  CHECK(VS::Battle::actionFlowMode == 6);
  CHECK(VS::Effects::actionVariant == 0);
  CHECK(fixture->state.queue_index == 0xFFFFFFFF);
}
TEST_CASE("F00188 production busy bit uses no missing action owner",
          "[action-sequence][F00188]") {
  auto fixture = std::make_unique<LiveFixture>();
  VS::Battle::actionState = nullptr;
  VS::Effects::effectBusyFlags = -1;
  VS::Battle::actionFlowMode = 19;
  b::StartLiveReadyActionAnimation();
  CHECK(VS::Battle::actionState == nullptr);
  CHECK(VS::Battle::actionFlowMode == 19);
}
TEST_CASE("F00192 production planning release and trail phase use real helpers",
          "[action-sequence][F00192][ai-completed-plan][action-phases]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->state.queue_index = 0xFFFFFFFE;
  fixture->state.queue[2].primary_target.targets_position = 1;
  fixture->room.readiness_level = 19;
  fixture->actor.command_state = 0x80;
  fixture->actor.command_opcode = 0x18;
  VS::Battle::actors[15] = &fixture->actor;
  b::WaitForLiveActionResolution();
  CHECK(fixture->room.readiness_level == 0);
  CHECK(fixture->actor.command_state == 8);
  CHECK(VS::Battle::actionFlowMode == 10);
  CHECK(fixture->state.queue_index == 0xFFFFFFFE);
}
TEST_CASE("F00193 production255 targets use real animation readiness",
          "[action-sequence][F00193][actors-animation]") {
  auto fixture = std::make_unique<LiveFixture>();
  fixture->state.chain_step = 1;
  fixture->state.queue[0].target_count = 255;
  for (auto& target : fixture->state.queue[0].targets) {
    target.target.target_actor = 15;
  }
  fixture->record.instance.action_animation_state = 1;
  VS::Actors::actorRecords[15] = &fixture->record;
  VS::Battle::actionFlowMode = 10;
  b::WaitForLiveTargetRecovery();
  CHECK(VS::Battle::actionFlowMode == 10);
  CHECK(fixture->state.queue[0].target_count == 255);
}
TEST_CASE(
    "F00195 production restoration uses the native renderer and signed "
    "preferred distance",
    "[action-sequence][F00195][camera-view]") {
  const ProjectionReset reset;
  auto fixture = std::make_unique<LiveFixture>();
  VS::Field::sphericalCamera.motion.zoom_mode = 3;
  VS::Field::sphericalCamera.values.distance = -32600;
  fixture->state.preferred_camera_distance = -32768;
  VS::Battle::settledCameraDistance = 12345;
  VS::Render::SetLiveCameraProjectionDistance(577);
  b::FinishLiveActionSequence();
  CHECK(VS::Render::projectionDistance == 513);
  CHECK(VS::Field::sphericalCamera.values.distance == -32768);
  CHECK(VS::Battle::settledCameraDistance == 12345);
  CHECK(fixture->state.chain_step == 0);
}
TEST_CASE(
    "F00195 production early projection gate leaves an unused owner absent",
    "[action-sequence][F00195]") {
  const ProjectionReset reset;
  auto fixture = std::make_unique<LiveFixture>();
  VS::Battle::actionState = nullptr;
  VS::Field::sphericalCamera.motion.zoom_mode = 0;
  VS::Render::SetLiveCameraProjectionDistance(511);
  b::FinishLiveActionSequence();
  CHECK(VS::Battle::actionState == nullptr);
  CHECK(VS::Render::projectionDistance == 511);
}
}  // namespace
