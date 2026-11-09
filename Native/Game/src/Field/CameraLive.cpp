// Hand-maintained (tools/port/owned.csv).
#include "Field/CameraLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorMovementLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/GameModeLive.hpp"
#include "Field/MapLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Legacy/Psx.h"
#include "Render/CameraViewLive.hpp"

extern "C" {
#include "Actors/Location.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/Sphere.h"
#include "Battle/TargetPages.h"
#include "Battle/TargetingState.h"
#include "Core/Gte.h"
#include "Core/Scratch.h"
#include "Field/Camera.h"
#include "Field/CameraLiveState.h"
#include "Field/GameModeLiveState.h"
#include "Field/Gimmicks.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Field/PlatformPlacementLive.h"
#include "Field/RoomLoadingLiveState.h"
#include "Field/Zone.h"
#include "Input/ControllerLiveState.h"
#include "Render/Camera.h"
#include "Render/Render.h"

// decomp: _firstPersonViewEnabled (_firstPersonViewEnabled@BATTLE/BATTLE.PRG)
int Field_isFirstPersonViewEnabled;
// decomp: vs_battle_cameraTransition
// (vs_battle_cameraTransition@BATTLE/BATTLE.PRG)
Field_CameraTransition* Field_cameraTransition;
// decomp: D_800F19C8 (D_800F19C8@BATTLE/BATTLE.PRG)
int Field_roomGeometryDrawMode;
// decomp: vs_battle_cameraCurrentSpherical
// (vs_battle_cameraCurrentSpherical@BATTLE/BATTLE.PRG)
Field_SphericalCamera Field_sphericalCamera;
}

namespace {
// The original's layouts (146C.h:738-823 and scratch.h:10).
constexpr std::size_t kSphericalValuesBytes = 0x10;
constexpr std::size_t kSphericalCameraBytes = 0x2C;
constexpr std::size_t kSphericalDeltaAt = 0x10;
constexpr std::size_t kSphericalSpeedAt = 0x28;
constexpr std::size_t kViewCameraBytes = 0x34;
constexpr std::size_t kViewCameraFarClipAt = 0x30;
constexpr std::size_t kTransitionBytes = 0xF8;
constexpr std::size_t kTransitionFromSphericalAt = 0x34;
constexpr std::size_t kTransitionToSphericalAt = 0x6C;
constexpr std::size_t kTransitionTargetPositionAt = 0xA4;
constexpr std::size_t kTransitionFramesAt = 0xF4;
}  // namespace

static_assert(sizeof(Field_SphericalCameraValues) == kSphericalValuesBytes);
static_assert(sizeof(Field_SphericalCamera) == kSphericalCameraBytes);
static_assert(offsetof(Field_SphericalCamera, delta) == kSphericalDeltaAt);
static_assert(offsetof(Field_SphericalCamera, speed) == kSphericalSpeedAt);
static_assert(sizeof(Render_Camera) == kViewCameraBytes);
static_assert(offsetof(Render_Camera, farClip) == kViewCameraFarClipAt);
static_assert(sizeof(Field_CameraTransition) == kTransitionBytes);
static_assert(offsetof(Field_CameraTransition, fromSpherical) ==
              kTransitionFromSphericalAt);
static_assert(offsetof(Field_CameraTransition, toSpherical) ==
              kTransitionToSphericalAt);
static_assert(offsetof(Field_CameraTransition, targetPosition) ==
              kTransitionTargetPositionAt);
static_assert(offsetof(Field_CameraTransition, transitionFrames) ==
              kTransitionFramesAt);

namespace VS::Field {
namespace {
namespace Math = VS::Core::Math;

constexpr std::string_view kChannel = "field.camera";
// Battle_ActionState::cameraPreview when no preview is framed.
constexpr std::uint8_t kNoCameraPreview = 0xFF;
// portInfo_t::controllerType of an analog (DualShock) pad.
constexpr int kAnalogControllerType = 7;

std::uint64_t refused_calls = 0;

// True on the 1st, 2nd, 4th, 8th... occurrence, to keep repeated
// diagnostics readable.
[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

void RefuseNull(std::string_view entry) {
  if (ShouldReport(refused_calls)) {
    VS_LOG_WARN(kChannel, "{} was given a null record; the call is refused",
                entry);
  }
}

[[nodiscard]] Render_FrameScratch& Scratch() {
  return *static_cast<Render_FrameScratch*>(Core_ScratchBase());
}

[[nodiscard]] Actors::MotionVector MotionOf(const SVECTOR& vector) {
  return {.x = vector.vx, .y = vector.vy, .z = vector.vz, .w = 0};
}

[[nodiscard]] ViewCamera ViewCameraOf(const Render_Camera& camera) {
  return {.position = Math::VectorOf(camera.position),
          .look_at = Math::VectorOf(camera.lookAt),
          .angles = Math::VectorOf(camera.angles),
          .far_clip = camera.farClip};
}

void StoreViewCamera(Render_Camera& record, const ViewCamera& camera) {
  Math::StoreVector(record.position, camera.position);
  Math::StoreVector(record.lookAt, camera.look_at);
  Math::StoreVector(record.angles, camera.angles);
  record.farClip = camera.far_clip;
}

[[nodiscard]] OrbitAngles AnglesOf(const Field_SphericalCameraValues& values) {
  return {.yaw = values.yaw,
          .pitch = values.pitch,
          .distance = values.distance,
          .mode = values.mode};
}

void StoreAngles(Field_SphericalCameraValues& record,
                 const OrbitAngles& angles) {
  record.yaw = angles.yaw;
  record.pitch = angles.pitch;
  record.distance = angles.distance;
  record.mode = angles.mode;
}

[[nodiscard]] SphericalCamera SphericalOf(const Field_SphericalCamera& orbit) {
  return {.values = AnglesOf(orbit.values),
          .motion = {.turn = orbit.delta.yaw,
                     .target_yaw = orbit.delta.pitch,
                     .turn_speed = orbit.delta.distance,
                     .zoom_mode = orbit.delta.mode},
          .preferred_distance = orbit.initialDistance,
          .zoom_velocity = orbit.zoomVelocity,
          .tracking_speed = orbit.speed};
}

void StoreSpherical(Field_SphericalCamera& record,
                    const SphericalCamera& orbit) {
  StoreAngles(record.values, orbit.values);
  record.delta.yaw = orbit.motion.turn;
  record.delta.pitch = orbit.motion.target_yaw;
  record.delta.distance = orbit.motion.turn_speed;
  record.delta.mode = orbit.motion.zoom_mode;
  record.initialDistance = orbit.preferred_distance;
  record.zoomVelocity = orbit.zoom_velocity;
  record.speed = orbit.tracking_speed;
}

[[nodiscard]] FirstPersonTransition TransitionOf(
    const Field_CameraTransition& record) {
  return {.from_camera = ViewCameraOf(record.fromCameraState),
          .from_spherical = SphericalOf(record.fromSpherical),
          .from_near_clip = record.fromNearClip,
          .from_projection_distance = record.fromProjectionDistance,
          .from_far_clip = record.fromFarClip,
          .to_spherical = SphericalOf(record.toSpherical),
          .to_near_clip = record.toNearClip,
          .to_projection_distance = record.toProjectionDistance,
          .to_far_clip = record.toFarClip,
          .target_position = Math::VectorOf(record.targetPosition),
          .target_look_at = Math::VectorOf(record.targetLookAt),
          .direction = Math::VectorOf(record.direction),
          .look_at_step = Math::VectorOf(record.lookAtStep),
          .far_clip_step = record.farClipStep,
          .near_clip_step = record.nearClipStep,
          .projection_distance_step = record.projectionDistanceStep,
          .speed = record.transitionSpeed,
          .frames = record.transitionFrames};
}

void StoreTransition(Field_CameraTransition& record,
                     const FirstPersonTransition& transition) {
  StoreViewCamera(record.fromCameraState, transition.from_camera);
  StoreSpherical(record.fromSpherical, transition.from_spherical);
  record.fromNearClip = transition.from_near_clip;
  record.fromProjectionDistance = transition.from_projection_distance;
  record.fromFarClip = transition.from_far_clip;
  StoreSpherical(record.toSpherical, transition.to_spherical);
  record.toNearClip = transition.to_near_clip;
  record.toProjectionDistance = transition.to_projection_distance;
  record.toFarClip = transition.to_far_clip;
  Math::StoreVector(record.targetPosition, transition.target_position);
  Math::StoreVector(record.targetLookAt, transition.target_look_at);
  Math::StoreVector(record.direction, transition.direction);
  Math::StoreVector(record.lookAtStep, transition.look_at_step);
  record.farClipStep = transition.far_clip_step;
  record.nearClipStep = transition.near_clip_step;
  record.projectionDistanceStep = transition.projection_distance_step;
  record.transitionSpeed = transition.speed;
  record.transitionFrames = transition.frames;
}

// Runs a rule over the loaded globals and stores them back before returning
// its result.
template <typename Rule>
auto RunLive(Rule rule) {
  FieldCameraState state = LoadLiveFieldCameraState();
  LiveCameraHost host(state);
  if constexpr (std::is_void_v<decltype(rule(host, state))>) {
    rule(host, state);
    StoreLiveFieldCameraState(state);
  } else {
    auto result = rule(host, state);
    StoreLiveFieldCameraState(state);
    return result;
  }
}
}  // namespace

FieldCameraState LoadLiveFieldCameraState() {
  const Render_FrameScratch& scratch = Scratch();
  FieldCameraState state{
      .camera = ViewCameraOf(scratch.camera),
      .spherical = SphericalOf(Field_sphericalCamera),
      .transition = std::nullopt,
      .first_person_view_enabled = Field_isFirstPersonViewEnabled,
      .geometry_draw_mode = Field_roomGeometryDrawMode,
      .view_facing_word = scratch.firstPersonViewFacing};
  if (Field_cameraTransition != nullptr) {
    state.transition = TransitionOf(*Field_cameraTransition);
  }
  return state;
}

void StoreLiveFieldCameraState(const FieldCameraState& state) {
  Render_FrameScratch& scratch = Scratch();
  StoreViewCamera(scratch.camera, state.camera);
  StoreSpherical(Field_sphericalCamera, state.spherical);
  if (state.transition && Field_cameraTransition != nullptr) {
    StoreTransition(*Field_cameraTransition, *state.transition);
  }
  Field_isFirstPersonViewEnabled = state.first_person_view_enabled;
  Field_roomGeometryDrawMode = state.geometry_draw_mode;
  scratch.firstPersonViewFacing = state.view_facing_word;
}

template <typename Call>
void LiveCameraHost::Escape(Call call) {
  StoreLiveFieldCameraState(state_);
  call();
  state_ = LoadLiveFieldCameraState();
}

std::optional<Actors::ActorPlacementView> LiveCameraHost::ActorPlacement(
    int actor_id) {
  return Actors::LiveActorPlacementView(actor_id);
}

void LiveCameraHost::UpdateRoomActorMovement() {
  Escape([] { Actors::UpdateLiveRoomActorMovement(); });
}

void LiveCameraHost::TurnActorTo(int actor_id, std::int32_t yaw) {
  Escape([actor_id, yaw] {
    Actors::TurnLiveActorToOverTime(actor_id, static_cast<std::uint16_t>(yaw),
                                    0);
  });
}

std::uint32_t LiveCameraHost::ActorStateFlags(int actor_id) {
  return Actors::LiveActorStateFlags(actor_id);
}

void LiveCameraHost::SetActorDisplayFlags(int actor_id, std::uint32_t flags) {
  Escape([actor_id, flags] {
    Actors::SetLiveActorDisplayFlags(actor_id, static_cast<int>(flags));
  });
}

void LiveCameraHost::RestoreActorIdlePose(int actor_id) {
  Escape([actor_id] { Actors::RestoreLiveActorIdlePose(actor_id); });
}

void LiveCameraHost::SetActorTransitionMode(int mode) {
  Escape([mode] { Actors::SetLiveActorTransitionMode(mode); });
}

bool LiveCameraHost::IsInteractionPaused() {
  return Field_isInteractionPaused != 0;
}

CameraBounds LiveCameraHost::ZoneBounds() {
  const Field_ZoneBounds& bounds = Field_zoneContext.bounds;
  return {.min_x = bounds.minX,
          .min_z = bounds.minZ,
          .max_x = bounds.maxX,
          .max_z = bounds.maxZ};
}

std::optional<std::uint8_t> LiveCameraHost::CollisionCellFlags(int tile_x,
                                                               int tile_z,
                                                               int layer) {
  const Field_MpdTileCell* const cell =
      LiveCollisionCellAt(static_cast<unsigned int>(tile_x),
                          static_cast<unsigned int>(tile_z), layer);
  if (cell == nullptr) return std::nullopt;
  return static_cast<std::uint8_t>(cell->occlusionAndFloorFlags);
}

void LiveCameraHost::ApplyGeometryOcclusion(int tile_x, int tile_z, int layer,
                                            bool far_camera) {
  Escape([tile_x, tile_z, layer, far_camera] {
    static_cast<void>(VS::Field::LiveApplyCameraGeometryOcclusion(
        tile_x, tile_z, layer, far_camera ? 1 : 0));
  });
}

void LiveCameraHost::ClearGeometryOcclusion() {
  Escape([] { VS::Field::LiveClearGeometryOcclusion(); });
}

void LiveCameraHost::SetMinimapVisible(bool visible) {
  Escape(
      [visible] { static_cast<void>(SetLiveMinimapVisible(visible ? 1 : 0)); });
}

void LiveCameraHost::SetMovingPlatformsPaused(bool paused) {
  Escape([paused] { Field_SetMovingPlatformsPausedEntry(paused ? 1 : 0); });
}

void LiveCameraHost::EnterFieldExploration() {
  Escape([] { LiveEnterFieldExplorationMode(); });
}

std::uint32_t LiveCameraHost::ControlMode() { return Field_controlMode; }

void LiveCameraHost::SetControlMode(std::uint32_t mode) {
  Field_controlMode = mode;
}

std::uint32_t LiveCameraHost::ActionFlowMode() { return Battle_actionFlowMode; }

void LiveCameraHost::SetActionFlowMode(std::uint32_t mode) {
  Battle_actionFlowMode = mode;
}

bool LiveCameraHost::TargetPagesUseShoulders() {
  return Battle_TargetPagesUseShoulders() != 0;
}

int LiveCameraHost::SphereInteractionMode() {
  return Battle_SphereInteractionMode();
}

// The core reports a missing action itself.
std::optional<ActionCameraTargets> LiveCameraHost::ActionTargets() {
  const Battle_ActionState* const action = Battle_actionState;
  if (action == nullptr) return std::nullopt;
  ActionCameraTargets targets{.preview = std::nullopt,
                              .area_center = MotionOf(action->areaCenter)};
  // previews has 255 entries, so every preview index but 0xFF is in range.
  if (action->cameraPreview != kNoCameraPreview && action->previewCount != 0) {
    targets.preview =
        MotionOf(action->previews[action->cameraPreview].position);
  }
  return targets;
}

void LiveCameraHost::SetPreferredActionCameraDistance(std::int32_t distance) {
  if (Battle_actionState == nullptr) {
    if (ShouldReport(refused_calls)) {
      VS_LOG_WARN(kChannel,
                  "no battle action to keep the preferred camera distance {}",
                  distance);
    }
    return;
  }
  Battle_actionState->preferredCameraDistance =
      static_cast<std::int16_t>(distance);
}

void LiveCameraHost::SetHudHidden(bool hidden) {
  Escape([hidden] { Battle_SetHudHidden(hidden ? 1 : 0); });
}

void LiveCameraHost::SetActorMarkersHidden(bool hidden) {
  Escape([hidden] { Battle_SetActorMarkersHidden(hidden ? 1 : 0); });
}

std::uint32_t LiveCameraHost::HeldButtons() {
  return static_cast<std::uint32_t>(Input_previousButtons);
}

std::uint32_t LiveCameraHost::PressedButtons() {
  return Input_buttonsPressed.allButtons;
}

std::optional<StickPosition> LiveCameraHost::AnalogRightStick() {
  const Input_ControllerPortState& port = Input_controllerPorts[0];
  if (static_cast<std::uint8_t>(port.controllerType) != kAnalogControllerType) {
    return std::nullopt;
  }
  return StickPosition{.x = static_cast<std::uint8_t>(port.rightStickX),
                       .y = static_cast<std::uint8_t>(port.rightStickY)};
}

// The original loads vs_scratch_t::viewMatrix at 0x1F800014; the native
// scratch keeps the field but not the PS1 offset.
std::int32_t LiveCameraHost::ProjectToScreen(
    const Actors::MotionVector& point) {
  Render_FrameScratch& scratch = Scratch();
  Core_Gte_SetRotMatrix(&scratch.viewMatrix);
  Core_Gte_SetTransMatrix(&scratch.viewMatrix);
  SVECTOR vector{.vx = point.x, .vy = point.y, .vz = point.z, .pad = 0};
  Core_GteWord screen_xy = 0;
  Core_GteWord interpolation = 0;
  Core_GteWord flag = 0;
  static_cast<void>(
      Core_Gte_RotTransPers(&vector, &screen_xy, &interpolation, &flag));
  return static_cast<std::int32_t>(screen_xy);
}

std::int32_t LiveCameraHost::NearClip() { return Render_nearClip; }

std::int32_t LiveCameraHost::ProjectionDistance() {
  return Render_projectionDistance;
}

void LiveCameraHost::SetNearClip(std::int32_t near_clip) {
  Escape([near_clip] { Render::SetLiveCameraNearClip(near_clip); });
}

void LiveCameraHost::SetProjectionDistance(std::int32_t distance) {
  Escape([distance] { Render::SetLiveCameraProjectionDistance(distance); });
}

std::optional<FirstPersonTransition> LiveCameraHost::AllocateTransition() {
  auto* const record = static_cast<Field_CameraTransition*>(
      Core::Memory::AllocateLiveFromHighEnd(sizeof(Field_CameraTransition)));
  if (record == nullptr) return std::nullopt;
  Field_cameraTransition = record;
  return TransitionOf(*record);
}

void LiveCameraHost::FreeTransition() {
  if (Field_cameraTransition != nullptr) {
    Core::Memory::FreeLiveFromHighEnd(Field_cameraTransition);
    Field_cameraTransition = nullptr;
  }
}

void LiveUpdateCameraFacingState() {
  RunLive([](CameraHost&, FieldCameraState& state) {
    UpdateCameraFacingState(state);
  });
}

int LiveAddCameraStepWithFallback(int base, int step, int remaining) {
  return AddCameraStepWithFallback(base, step, remaining);
}

int LiveAddClampedCameraStep(int base, int step, int remaining) {
  return AddClampedCameraStep(base, step, remaining);
}

void LiveClampCameraLookAtToZone(const VECTOR* in_position,
                                 VECTOR* out_position) {
  if (in_position == nullptr || out_position == nullptr) {
    RefuseNull("ClampCameraLookAtToZone");
    return;
  }
  const CameraVector position = Math::VectorOf(*in_position);
  const CameraVector clamped =
      RunLive([&position](CameraHost& host, FieldCameraState& state) {
        return ClampCameraLookAtToZone(host, state, position);
      });
  Math::StoreVector(*out_position, clamped);
}

int LiveComputeCameraTrackingStep(const SVECTOR* target_position,
                                  VECTOR* out_step, VECTOR* out_displacement,
                                  int clamp_to_zone, int max_speed) {
  if (target_position == nullptr || out_step == nullptr ||
      out_displacement == nullptr) {
    RefuseNull("ComputeCameraTrackingStep");
    return 0;
  }
  const Actors::MotionVector target = MotionOf(*target_position);
  const CameraTrackingStep step =
      RunLive([&target, clamp_to_zone, max_speed](CameraHost& host,
                                                  FieldCameraState& state) {
        return ComputeCameraTrackingStep(host, state, target,
                                         clamp_to_zone != 0, max_speed);
      });
  Math::StoreVector(*out_displacement, step.displacement);
  Math::StoreVector(*out_step, step.step);
  return step.not_centered ? 1 : 0;
}

int LiveStepCameraLookAtTowardTarget(const SVECTOR* target_position,
                                     int clamp_to_zone, int max_speed) {
  if (target_position == nullptr) {
    RefuseNull("StepCameraLookAtTowardTarget");
    return 0;
  }
  const Actors::MotionVector target = MotionOf(*target_position);
  return RunLive([&target, clamp_to_zone, max_speed](CameraHost& host,
                                                     FieldCameraState& state) {
    return StepCameraLookAtTowardTarget(host, state, target, clamp_to_zone != 0,
                                        max_speed);
  });
}

void LiveTrackCameraLookAtWithinDeadZone(const VECTOR* target_position) {
  if (target_position == nullptr) {
    RefuseNull("TrackCameraLookAtWithinDeadZone");
    return;
  }
  const CameraVector target = Math::VectorOf(*target_position);
  RunLive([&target](CameraHost& host, FieldCameraState& state) {
    TrackCameraLookAtWithinDeadZone(host, state, target);
  });
}

void LiveUpdateCameraOrbitInput() {
  RunLive([](CameraHost& host, FieldCameraState& state) {
    UpdateCameraOrbitInput(host, state);
  });
}

void LiveTrackActionCameraTarget(int tracked_actor_index) {
  RunLive([tracked_actor_index](CameraHost& host, FieldCameraState& state) {
    TrackActionCameraTarget(host, state, tracked_actor_index);
  });
}

int LiveUpdateAshleyFollowCamera() {
  RunLive([](CameraHost& host, FieldCameraState& state) {
    UpdateAshleyFollowCamera(host, state);
  });
  return 0;
}

void LiveUpdateFirstPersonCamera() {
  RunLive([](CameraHost& host, FieldCameraState& state) {
    UpdateFirstPersonCamera(host, state);
  });
}

void LiveStorePreferredActionCameraDistance() {
  RunLive([](CameraHost& host, FieldCameraState& state) {
    StorePreferredActionCameraDistance(host, state);
  });
}

void LiveComputeFirstPersonCameraTransition(const VECTOR* target_position) {
  if (target_position == nullptr) {
    RefuseNull("ComputeFirstPersonCameraTransition");
    return;
  }
  const CameraVector target = Math::VectorOf(*target_position);
  RunLive([&target](CameraHost& host, FieldCameraState& state) {
    ComputeFirstPersonCameraTransition(host, state, target);
  });
}

int LiveStepFirstPersonCameraTransition() {
  return RunLive([](CameraHost& host, FieldCameraState& state) {
    return StepFirstPersonCameraTransition(host, state);
  })
             ? 1
             : 0;
}

void LiveBeginFirstPersonView() {
  RunLive([](CameraHost& host, FieldCameraState& state) {
    BeginFirstPersonView(host, state);
  });
}

void LiveEndFirstPersonView() {
  RunLive([](CameraHost& host, FieldCameraState& state) {
    EndFirstPersonView(host, state);
  });
}

void LiveUpdateFirstPersonViewEntry() {
  RunLive([](CameraHost& host, FieldCameraState& state) {
    UpdateFirstPersonViewEntry(host, state);
  });
}

void LiveCheckFirstPersonViewExit() {
  RunLive([](CameraHost& host, FieldCameraState& state) {
    CheckFirstPersonViewExit(host, state);
  });
}

void LiveUpdateFirstPersonViewExit() {
  RunLive([](CameraHost& host, FieldCameraState& state) {
    UpdateFirstPersonViewExit(host, state);
  });
}

void LiveUpdateCameraTileConstraint(int actor_index,
                                    const Actors_TileTarget* actor_tile) {
  if (actor_tile == nullptr) {
    RefuseNull("UpdateCameraTileConstraint");
    return;
  }
  const Actors::PackedTile tile{
      .x = static_cast<std::uint8_t>(actor_tile->tileX),
      .layer = static_cast<std::uint8_t>(actor_tile->tileLayer),
      .z = static_cast<std::uint8_t>(actor_tile->tileZ),
      .spare = static_cast<std::uint8_t>(actor_tile->facing)};
  RunLive([actor_index, &tile](CameraHost& host, FieldCameraState& state) {
    UpdateCameraTileConstraint(host, state, actor_index, tile);
  });
}

int LiveIsCameraLookAtOnAshley() {
  return RunLive([](CameraHost& host, FieldCameraState& state) {
    return IsCameraLookAtOnAshley(host, state);
  })
             ? 1
             : 0;
}

void LiveResetCameraToAshley() {
  RunLive([](CameraHost& host, FieldCameraState& state) {
    ResetCameraToAshley(host, state);
  });
}

int LiveSyncCameraAnglesFromPosition(Field_SphericalCamera* output) {
  OrbitAngles angles{};
  if (output != nullptr) angles = AnglesOf(output->values);
  const std::int32_t yaw =
      RunLive([output, &angles](CameraHost&, FieldCameraState& state) {
        return SyncCameraAnglesFromPosition(
            state, output != nullptr ? &angles : nullptr);
      });
  if (output != nullptr) StoreAngles(output->values, angles);
  return yaw;
}

void LiveComputeSphericalCameraPosition(
    VECTOR* out_position, const VECTOR* base_position,
    const Field_SphericalCameraValues* angles) {
  if (out_position == nullptr || base_position == nullptr ||
      angles == nullptr) {
    RefuseNull("ComputeSphericalCameraPosition");
    return;
  }
  const OrbitAngles orbit = AnglesOf(*angles);
  CameraVector base = Math::VectorOf(*base_position);
  if (out_position == base_position) {
    // The original normalizes the direction into the output before adding
    // the base, so an aliased base is the unit direction itself.
    const OrbitAngles unit{.yaw = orbit.yaw,
                           .pitch = orbit.pitch,
                           .distance = 1,
                           .mode = orbit.mode};
    base = ComputeSphericalCameraPosition(CameraVector{}, unit);
  }
  Math::StoreVector(*out_position, ComputeSphericalCameraPosition(base, orbit));
}

void LiveSetCameraPositionFromAngles(
    const Field_SphericalCameraValues* angles) {
  if (angles == nullptr) {
    RefuseNull("SetCameraPositionFromAngles");
    return;
  }
  const OrbitAngles orbit = AnglesOf(*angles);
  RunLive([&orbit](CameraHost&, FieldCameraState& state) {
    SetCameraPositionFromAngles(state, orbit);
  });
}

void LiveSetCameraLookAtFromAngles(const Field_SphericalCameraValues* angles) {
  if (angles == nullptr) {
    RefuseNull("SetCameraLookAtFromAngles");
    return;
  }
  const OrbitAngles orbit = AnglesOf(*angles);
  RunLive([&orbit](CameraHost&, FieldCameraState& state) {
    SetCameraLookAtFromAngles(state, orbit);
  });
}

void LiveCameraPosition(VECTOR* out_position) {
  if (out_position == nullptr) {
    RefuseNull("CameraPosition");
    return;
  }
  Math::StoreVector(*out_position, CameraPosition(LoadLiveFieldCameraState()));
}

void LiveSetCameraPosition(const VECTOR* in_position) {
  if (in_position == nullptr) {
    RefuseNull("SetCameraPosition");
    return;
  }
  const CameraVector position = Math::VectorOf(*in_position);
  RunLive([&position](CameraHost&, FieldCameraState& state) {
    SetCameraPosition(state, position);
  });
}

void LiveCameraLookAt(VECTOR* out_look_at) {
  if (out_look_at == nullptr) {
    RefuseNull("CameraLookAt");
    return;
  }
  Math::StoreVector(*out_look_at, CameraLookAt(LoadLiveFieldCameraState()));
}

void LiveSetCameraLookAt(const VECTOR* in_look_at) {
  if (in_look_at == nullptr) {
    RefuseNull("SetCameraLookAt");
    return;
  }
  const CameraVector look_at = Math::VectorOf(*in_look_at);
  RunLive([&look_at](CameraHost&, FieldCameraState& state) {
    SetCameraLookAt(state, look_at);
  });
}

void LiveSetCameraRoll(int roll_angle) {
  RunLive([roll_angle](CameraHost&, FieldCameraState& state) {
    SetCameraRoll(state, roll_angle);
  });
}

int LiveCameraRoll() { return CameraRoll(LoadLiveFieldCameraState()); }

int LiveSetFirstPersonViewDisabled(int disabled) {
  return RunLive([disabled](CameraHost& host, FieldCameraState& state) {
    return SetFirstPersonViewDisabled(host, state, disabled);
  });
}

void LiveInitializeSphericalCamera(VECTOR* out_position, VECTOR* out_look_at,
                                   int facing, int mode) {
  if (out_position == nullptr || out_look_at == nullptr) {
    RefuseNull("InitializeSphericalCamera");
    return;
  }
  const bool look_at_is_view_camera = out_look_at == &Scratch().camera.lookAt;
  const CameraPlacement placement =
      RunLive([facing, mode, look_at_is_view_camera](CameraHost& host,
                                                     FieldCameraState& state) {
        return InitializeSphericalCamera(host, state, facing, mode,
                                         look_at_is_view_camera);
      });
  Math::StoreVector(*out_look_at, placement.look_at);
  Math::StoreVector(*out_position, placement.position);
}

void LiveSetCameraPitchPreset(int requested_pitch) {
  RunLive([requested_pitch](CameraHost&, FieldCameraState& state) {
    SetCameraPitchPreset(state, requested_pitch);
  });
}
}  // namespace VS::Field
