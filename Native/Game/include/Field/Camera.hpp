#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Actors/ActorPlacement.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/CollisionMap.hpp"

// The field camera (BATTLE.PRG 146C.c:776-9106): the orbit camera that
// follows Ashley and the battle action's targets, its zone clamp, tracking
// speed, quarter turns and zoom, and the eased transition into and out of the
// first-person view. The rules work on a FieldCameraState value and reach the
// actors, input, battle flow, renderer and heap only through CameraHost.
namespace VS::Field {
using CameraVector = Core::Math::Vec3;

// Controller word bits (Legacy/Input/LibPad.h).
inline constexpr std::uint32_t kPadL1 = 1U << 2;
inline constexpr std::uint32_t kPadR1 = 1U << 3;
inline constexpr std::uint32_t kPadCross = 1U << 6;
inline constexpr std::uint32_t kPadSelect = 1U << 8;
inline constexpr std::uint32_t kPadRightStickButton = 1U << 10;
inline constexpr std::uint32_t kPadStart = 1U << 11;
inline constexpr std::uint32_t kPadUp = 1U << 12;
inline constexpr std::uint32_t kPadRight = 1U << 13;
inline constexpr std::uint32_t kPadDown = 1U << 14;
inline constexpr std::uint32_t kPadLeft = 1U << 15;

// Field_controlMode values the camera tests.
inline constexpr std::uint32_t kControlModeAction = 2;
inline constexpr std::uint32_t kControlModeLocked = 3;
inline constexpr std::uint32_t kControlModeFirstPerson = 7;
// Battle_actionFlowMode values in the action control mode...
inline constexpr std::uint32_t kFlowChooseDirectTarget = 1;
inline constexpr std::uint32_t kFlowPlaceArea = 2;
inline constexpr std::uint32_t kFlowChooseAreaTarget = 3;
inline constexpr std::uint32_t kFlowActionLocked = 4;
inline constexpr std::uint32_t kFlowFollowActor = 11;
// ...and in the first-person one.
inline constexpr std::uint32_t kFirstPersonEntering = 0;
inline constexpr std::uint32_t kFirstPersonLooking = 1;
inline constexpr std::uint32_t kFirstPersonLeaving = 2;

// The orbit's zoom modes (delta.mode): zooming in to and resting at the near
// distance, zooming out to and resting at the far one.
inline constexpr std::int32_t kZoomingIn = 0;
inline constexpr std::int32_t kNearZoom = 1;
inline constexpr std::int32_t kZoomingOut = 2;
inline constexpr std::int32_t kFarZoom = 3;
inline constexpr std::int32_t kNearCameraDistance = 0x600;
inline constexpr std::int32_t kFarCameraDistance = 0x900;

// The quarter turn in progress (delta.yaw).
inline constexpr std::int32_t kNoTurn = 0;
inline constexpr std::int32_t kTurningRight = 1;
inline constexpr std::int32_t kTurningLeft = 2;

// _sphericalValues: the orbit's angles (4096 per turn) and distance.
struct OrbitAngles {
  std::int32_t yaw = 0;
  std::int32_t pitch = 0;
  std::int32_t distance = 0;
  // Never read by the camera; kept with the record.
  std::int32_t mode = 0;

  friend bool operator==(const OrbitAngles&, const OrbitAngles&) = default;
};

// The second _sphericalValues: the quarter turn and zoom in progress.
struct OrbitMotion {
  std::int32_t turn = kNoTurn;          // delta.yaw
  std::int32_t target_yaw = 0;          // delta.pitch
  std::int32_t turn_speed = 0;          // delta.distance
  std::int32_t zoom_mode = kZoomingIn;  // delta.mode

  friend bool operator==(const OrbitMotion&, const OrbitMotion&) = default;
};

// _sphericalCamera.
struct SphericalCamera {
  OrbitAngles values;
  OrbitMotion motion;
  // The distance the zoom eases toward (initialDistance).
  std::int32_t preferred_distance = 0;
  std::int32_t zoom_velocity = 0;
  // The look-at tracking speed (speed).
  std::int32_t tracking_speed = 0;

  friend bool operator==(const SphericalCamera&,
                         const SphericalCamera&) = default;
};

// The frame scratch camera (vs_scratch_t::camera): Q12 world position and
// look-at, angles (x pitch, y yaw, z roll) and far clip.
struct ViewCamera {
  CameraVector position;
  CameraVector look_at;
  CameraVector angles;
  std::int32_t far_clip = 0;

  friend bool operator==(const ViewCamera&, const ViewCamera&) = default;
};

// vs_battle_cameraTransition_t: the camera saved on entering the first-person
// view, the pose it eases to and the per-frame steps.
struct FirstPersonTransition {
  ViewCamera from_camera;
  SphericalCamera from_spherical;
  std::int32_t from_near_clip = 0;
  std::int32_t from_projection_distance = 0;
  std::int32_t from_far_clip = 0;
  SphericalCamera to_spherical;
  std::int32_t to_near_clip = 0;
  std::int32_t to_projection_distance = 0;
  std::int32_t to_far_clip = 0;
  CameraVector target_position;
  CameraVector target_look_at;
  CameraVector direction;
  CameraVector look_at_step;
  std::int32_t far_clip_step = 0;
  std::int32_t near_clip_step = 0;
  std::int32_t projection_distance_step = 0;
  std::int32_t speed = 0;
  std::int32_t frames = 0;

  friend bool operator==(const FirstPersonTransition&,
                         const FirstPersonTransition&) = default;
};

// Everything the camera rules own.
struct FieldCameraState {
  ViewCamera camera;
  SphericalCamera spherical;
  // Allocated on entering the first-person view, freed on leaving it.
  std::optional<FirstPersonTransition> transition;
  std::int32_t first_person_view_enabled = 0;
  // 1 while the first-person view hides the room's occluding geometry.
  std::int32_t geometry_draw_mode = 0;
  // vs_scratch_t::firstPersonViewFacing.
  std::uint32_t view_facing_word = 0;

  friend bool operator==(const FieldCameraState&,
                         const FieldCameraState&) = default;
};

// The step and remaining displacement toward a tracking target.
struct CameraTrackingStep {
  bool not_centered = false;
  CameraVector step;
  CameraVector displacement;

  friend bool operator==(const CameraTrackingStep&,
                         const CameraTrackingStep&) = default;
};

// A camera position and the point it looks at.
struct CameraPlacement {
  CameraVector position;
  CameraVector look_at;

  friend bool operator==(const CameraPlacement&,
                         const CameraPlacement&) = default;
};

// The battle action's camera targets: the previewed target (when one is
// chosen) and the area-of-effect centre.
struct ActionCameraTargets {
  std::optional<Actors::MotionVector> preview;
  Actors::MotionVector area_center;
};

// The analog right stick, 0..255 per axis with 128 centred.
struct StickPosition {
  std::uint8_t x = 0;
  std::uint8_t y = 0;
};

class CameraHost {
 public:
  CameraHost() = default;
  CameraHost(const CameraHost&) = delete;
  CameraHost& operator=(const CameraHost&) = delete;
  CameraHost(CameraHost&&) = delete;
  CameraHost& operator=(CameraHost&&) = delete;
  virtual ~CameraHost() = default;

  // Actors.
  [[nodiscard]] virtual std::optional<Actors::ActorPlacementView>
  ActorPlacement(int actor_id) = 0;
  virtual void UpdateRoomActorMovement() = 0;
  virtual void TurnActorTo(int actor_id, std::int32_t yaw) = 0;
  [[nodiscard]] virtual std::uint32_t ActorStateFlags(int actor_id) = 0;
  virtual void SetActorDisplayFlags(int actor_id, std::uint32_t flags) = 0;
  virtual void RestoreActorIdlePose(int actor_id) = 0;
  virtual void SetActorTransitionMode(int mode) = 0;

  // The room.
  [[nodiscard]] virtual bool IsInteractionPaused() = 0;
  [[nodiscard]] virtual CameraBounds ZoneBounds() = 0;
  // The collision cell's occlusion and floor flags; nothing for a tile with
  // no authored cell.
  [[nodiscard]] virtual std::optional<std::uint8_t> CollisionCellFlags(
      int tile_x, int tile_z, int layer) = 0;
  virtual void ApplyGeometryOcclusion(int tile_x, int tile_z, int layer,
                                      bool far_camera) = 0;
  virtual void ClearGeometryOcclusion() = 0;
  virtual void SetMinimapVisible(bool visible) = 0;
  virtual void SetMovingPlatformsPaused(bool paused) = 0;
  virtual void EnterFieldExploration() = 0;

  // The battle flow.
  [[nodiscard]] virtual std::uint32_t ControlMode() = 0;
  virtual void SetControlMode(std::uint32_t mode) = 0;
  [[nodiscard]] virtual std::uint32_t ActionFlowMode() = 0;
  virtual void SetActionFlowMode(std::uint32_t mode) = 0;
  [[nodiscard]] virtual bool TargetPagesUseShoulders() = 0;
  [[nodiscard]] virtual int SphereInteractionMode() = 0;
  // Nothing when no battle action is set up.
  [[nodiscard]] virtual std::optional<ActionCameraTargets> ActionTargets() = 0;
  virtual void SetPreferredActionCameraDistance(std::int32_t distance) = 0;
  virtual void SetHudHidden(bool hidden) = 0;
  virtual void SetActorMarkersHidden(bool hidden) = 0;

  // Input.
  [[nodiscard]] virtual std::uint32_t HeldButtons() = 0;
  [[nodiscard]] virtual std::uint32_t PressedButtons() = 0;
  // The right stick when an analog controller is plugged in.
  [[nodiscard]] virtual std::optional<StickPosition> AnalogRightStick() = 0;

  // The renderer: the packed screen position (y << 16 | x) of a world point
  // through the view matrix, and the projection clip planes.
  [[nodiscard]] virtual std::int32_t ProjectToScreen(
      const Actors::MotionVector& point) = 0;
  [[nodiscard]] virtual std::int32_t NearClip() = 0;
  [[nodiscard]] virtual std::int32_t ProjectionDistance() = 0;
  virtual void SetNearClip(std::int32_t near_clip) = 0;
  virtual void SetProjectionDistance(std::int32_t distance) = 0;

  // The transition's heap record: a new one holds whatever the storage held,
  // nothing when the heap is full.
  [[nodiscard]] virtual std::optional<FirstPersonTransition>
  AllocateTransition() = 0;
  virtual void FreeTransition() = 0;
};

// decomp: BATTLE.PRG func_80069D78 (src/BATTLE/BATTLE.PRG/146C.c:776)
void UpdateCameraFacingState(FieldCameraState& state);
// decomp: BATTLE.PRG _addMinDelta (src/BATTLE/BATTLE.PRG/146C.c:2389)
// base + step, unless the step is zero or overshoots the remaining distance,
// which is then added instead.
[[nodiscard]] std::int32_t AddCameraStepWithFallback(std::int32_t base,
                                                     std::int32_t step,
                                                     std::int32_t remaining);
// decomp: BATTLE.PRG _add_min (src/BATTLE/BATTLE.PRG/146C.c:2409)
// base + whichever of step and remaining is nearer zero on remaining's side.
[[nodiscard]] std::int32_t AddClampedCameraStep(std::int32_t base,
                                                std::int32_t step,
                                                std::int32_t remaining);
// decomp: BATTLE.PRG _clampPositionToZoneBounds (146C.c:2426)
// Keeps a Q12 look-at inside the zone's bounds less the orbit's radius,
// snapped to whole units, and holds its height unless it moved far.
[[nodiscard]] CameraVector ClampCameraLookAtToZone(
    CameraHost& host, const FieldCameraState& state,
    const CameraVector& position);
// decomp: BATTLE.PRG _computeStepAndDisplacement (146C.c:2481)
// `target` is in world units.
[[nodiscard]] CameraTrackingStep ComputeCameraTrackingStep(
    CameraHost& host, FieldCameraState& state,
    const Actors::MotionVector& target, bool clamp_to_zone,
    std::int32_t max_speed);
// decomp: BATTLE.PRG _stepTowardTarget (src/BATTLE/BATTLE.PRG/146C.c:2569)
// Returns the tracking speed.
std::int32_t StepCameraLookAtTowardTarget(CameraHost& host,
                                          FieldCameraState& state,
                                          const Actors::MotionVector& target,
                                          bool clamp_to_zone,
                                          std::int32_t max_speed);
// decomp: BATTLE.PRG _snapLookAtToPosition (146C.c:2597)
void TrackCameraLookAtWithinDeadZone(CameraHost& host, FieldCameraState& state,
                                     const CameraVector& target);
// decomp: BATTLE.PRG func_8006E158 (src/BATTLE/BATTLE.PRG/146C.c:2641)
void UpdateCameraOrbitInput(CameraHost& host, FieldCameraState& state);
// decomp: BATTLE.PRG func_8006E640 (src/BATTLE/BATTLE.PRG/146C.c:2794)
void TrackActionCameraTarget(CameraHost& host, FieldCameraState& state,
                             int actor_id);
// decomp: BATTLE.PRG func_8006EBF8 (src/BATTLE/BATTLE.PRG/146C.c:2965)
void UpdateAshleyFollowCamera(CameraHost& host, FieldCameraState& state);
// decomp: BATTLE.PRG func_8006EC7C (src/BATTLE/BATTLE.PRG/146C.c:2984)
void UpdateFirstPersonCamera(CameraHost& host, FieldCameraState& state);
// decomp: BATTLE.PRG func_8006EF10 (src/BATTLE/BATTLE.PRG/146C.c:3044)
void StorePreferredActionCameraDistance(CameraHost& host,
                                        const FieldCameraState& state);
// decomp: BATTLE.PRG _computeCameraTransition (146C.c:3058)
// Refused (logged) without a transition record.
void ComputeFirstPersonCameraTransition(CameraHost& host,
                                        FieldCameraState& state,
                                        const CameraVector& target);
// decomp: BATTLE.PRG _stepCameraTransition (146C.c:3121)
// True once the camera reaches the target height. Refused (logged, false)
// without a transition record.
bool StepFirstPersonCameraTransition(CameraHost& host, FieldCameraState& state);
// decomp: BATTLE.PRG _initBattleCameraTransition (146C.c:4544)
// Refused (logged) when the heap cannot hold the transition.
void BeginFirstPersonView(CameraHost& host, FieldCameraState& state);
// decomp: BATTLE.PRG _endBattleCameraTransition (146C.c:4591)
// Without a transition record the camera is left as it is (logged).
void EndFirstPersonView(CameraHost& host, FieldCameraState& state);
// decomp: BATTLE.PRG func_80074580 (src/BATTLE/BATTLE.PRG/146C.c:5165)
void UpdateFirstPersonViewEntry(CameraHost& host, FieldCameraState& state);
// decomp: BATTLE.PRG _checkFirstPersonViewExit (146C.c:5179)
// Refused (logged) without a transition record.
void CheckFirstPersonViewExit(CameraHost& host, FieldCameraState& state);
// decomp: BATTLE.PRG func_80074744 (src/BATTLE/BATTLE.PRG/146C.c:5222)
void UpdateFirstPersonViewExit(CameraHost& host, FieldCameraState& state);
// decomp: BATTLE.PRG func_80074B14 (src/BATTLE/BATTLE.PRG/146C.c:5339)
void UpdateCameraTileConstraint(CameraHost& host, const FieldCameraState& state,
                                int actor_id, const Actors::PackedTile& tile);
// decomp: BATTLE.PRG _isLookAtAtDestination (146C.c:6788)
[[nodiscard]] bool IsCameraLookAtOnAshley(CameraHost& host,
                                          const FieldCameraState& state);
// decomp: BATTLE.PRG func_80077EC4 (src/BATTLE/BATTLE.PRG/146C.c:6811)
void ResetCameraToAshley(CameraHost& host, FieldCameraState& state);
// decomp: BATTLE.PRG vs_battle_syncCameraAnglesFromPosition (146C.c:8057)
// Sets the camera's yaw and pitch from its position and look-at and returns
// the yaw; `orbit`, when given, also receives them and the distance.
std::int32_t SyncCameraAnglesFromPosition(FieldCameraState& state,
                                          OrbitAngles* orbit);
// decomp: BATTLE.PRG _computeSphericalOffset (146C.c:8092)
[[nodiscard]] CameraVector ComputeSphericalCameraPosition(
    const CameraVector& base, const OrbitAngles& angles);
// decomp: BATTLE.PRG _setCameraPositionFromAngles (146C.c:8104)
void SetCameraPositionFromAngles(FieldCameraState& state,
                                 const OrbitAngles& angles);
// decomp: BATTLE.PRG _setCameraLookAtFromAngles (146C.c:8110)
void SetCameraLookAtFromAngles(FieldCameraState& state,
                               const OrbitAngles& angles);
// decomp: BATTLE.PRG vs_battle_getCameraPosition (146C.c:8125)
[[nodiscard]] CameraVector CameraPosition(const FieldCameraState& state);
// decomp: BATTLE.PRG vs_battle_setCameraPosition (146C.c:8132)
void SetCameraPosition(FieldCameraState& state, const CameraVector& position);
// decomp: BATTLE.PRG vs_battle_getCameraLookAt (146C.c:8139)
[[nodiscard]] CameraVector CameraLookAt(const FieldCameraState& state);
// decomp: BATTLE.PRG vs_battle_setCameraLookAt (146C.c:8146)
void SetCameraLookAt(FieldCameraState& state, const CameraVector& look_at);
// decomp: BATTLE.PRG vs_battle_setCameraRoll (146C.c:8153)
void SetCameraRoll(FieldCameraState& state, std::int32_t roll);
// decomp: BATTLE.PRG vs_battle_getCameraRoll (146C.c:8155)
[[nodiscard]] std::int32_t CameraRoll(const FieldCameraState& state);
// decomp: BATTLE.PRG func_8007BDD8 (src/BATTLE/BATTLE.PRG/146C.c:8573)
// Returns the new enabled flag.
std::int32_t SetFirstPersonViewDisabled(CameraHost& host,
                                        FieldCameraState& state,
                                        std::int32_t disabled);
// decomp: BATTLE.PRG vs_battle_initialiseCameraFromSpherical (146C.c:9071)
// A facing of -1 keeps the yaw; mode 1 selects the near distance and 2 the
// far one. When `look_at_is_view_camera`, the look-at is written to the view
// camera before it is clamped (the original's aliasing for
// ResetCameraToAshley).
[[nodiscard]] CameraPlacement InitializeSphericalCamera(
    CameraHost& host, FieldCameraState& state, std::int32_t facing,
    std::int32_t mode, bool look_at_is_view_camera);
// decomp: BATTLE.PRG func_8007CE74 (src/BATTLE/BATTLE.PRG/146C.c:9106)
void SetCameraPitchPreset(FieldCameraState& state, std::int32_t pitch);
}  // namespace VS::Field
