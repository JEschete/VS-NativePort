#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Field/Camera.hpp"
#include "Legacy/Psx.h"

struct Actors_TileTarget;
struct Field_SphericalCamera;
struct Field_SphericalCameraValues;

// Live binding for the field camera (Camera.hpp) over the copied camera
// globals (CameraLiveState.h) and the frame scratch camera: each entry loads
// them into a FieldCameraState, runs the typed core over the live actors,
// input, battle flow, renderer and high-end heap, stores them back and only
// then writes its output records. The entries keep the original call shapes
// (BATTLE.PRG 146C.c:776-9106) for the generated battle, script, sphere and
// room callers; a null record is refused with a diagnostic.
namespace VS::Field {
// The camera globals as a typed value, and back. Storing keeps the VECTOR
// pads and writes the transition only while one is allocated.
[[nodiscard]] FieldCameraState LoadLiveFieldCameraState();
void StoreLiveFieldCameraState(const FieldCameraState& state);

// The host over the live systems; calls that may reach other camera users
// store the state first and reload it afterwards.
class LiveCameraHost final : public CameraHost {
 public:
  explicit LiveCameraHost(FieldCameraState& state) : state_(state) {}

  [[nodiscard]] std::optional<Actors::ActorPlacementView> ActorPlacement(
      int actor_id) override;
  void UpdateRoomActorMovement() override;
  void TurnActorTo(int actor_id, std::int32_t yaw) override;
  [[nodiscard]] std::uint32_t ActorStateFlags(int actor_id) override;
  void SetActorDisplayFlags(int actor_id, std::uint32_t flags) override;
  void RestoreActorIdlePose(int actor_id) override;
  void SetActorTransitionMode(int mode) override;

  [[nodiscard]] bool IsInteractionPaused() override;
  [[nodiscard]] CameraBounds ZoneBounds() override;
  [[nodiscard]] std::optional<std::uint8_t> CollisionCellFlags(
      int tile_x, int tile_z, int layer) override;
  void ApplyGeometryOcclusion(int tile_x, int tile_z, int layer,
                              bool far_camera) override;
  void ClearGeometryOcclusion() override;
  void SetMinimapVisible(bool visible) override;
  void SetMovingPlatformsPaused(bool paused) override;
  void EnterFieldExploration() override;

  [[nodiscard]] std::uint32_t ControlMode() override;
  void SetControlMode(std::uint32_t mode) override;
  [[nodiscard]] std::uint32_t ActionFlowMode() override;
  void SetActionFlowMode(std::uint32_t mode) override;
  [[nodiscard]] bool TargetPagesUseShoulders() override;
  [[nodiscard]] int SphereInteractionMode() override;
  [[nodiscard]] std::optional<ActionCameraTargets> ActionTargets() override;
  void SetPreferredActionCameraDistance(std::int32_t distance) override;
  void SetHudHidden(bool hidden) override;
  void SetActorMarkersHidden(bool hidden) override;

  [[nodiscard]] std::uint32_t HeldButtons() override;
  [[nodiscard]] std::uint32_t PressedButtons() override;
  [[nodiscard]] std::optional<StickPosition> AnalogRightStick() override;

  [[nodiscard]] std::int32_t ProjectToScreen(
      const Actors::MotionVector& point) override;
  [[nodiscard]] std::int32_t NearClip() override;
  [[nodiscard]] std::int32_t ProjectionDistance() override;
  void SetNearClip(std::int32_t near_clip) override;
  void SetProjectionDistance(std::int32_t distance) override;

  [[nodiscard]] std::optional<FirstPersonTransition> AllocateTransition()
      override;
  void FreeTransition() override;

 private:
  // Stores the state, runs `call` and reloads the state.
  template <typename Call>
  void Escape(Call call);

  FieldCameraState& state_;
};

// func_80069D78 (146C.c:776).
void LiveUpdateCameraFacingState();
// _addMinDelta (146C.c:2389).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
[[nodiscard]] int LiveAddCameraStepWithFallback(int base, int step,
                                                int remaining);
// _add_min (146C.c:2409).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
[[nodiscard]] int LiveAddClampedCameraStep(int base, int step, int remaining);
// _clampPositionToZoneBounds (146C.c:2426). The records may alias.
void LiveClampCameraLookAtToZone(const VECTOR* in_position,
                                 VECTOR* out_position);
// _computeStepAndDisplacement (146C.c:2481): 1 when the target is off the
// screen's centre.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LiveComputeCameraTrackingStep(const SVECTOR* target_position,
                                  VECTOR* out_step, VECTOR* out_displacement,
                                  int clamp_to_zone, int max_speed);
// _stepTowardTarget (146C.c:2569): the tracking speed.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LiveStepCameraLookAtTowardTarget(const SVECTOR* target_position,
                                     int clamp_to_zone, int max_speed);
// _snapLookAtToPosition (146C.c:2597).
void LiveTrackCameraLookAtWithinDeadZone(const VECTOR* target_position);
// func_8006E158 (146C.c:2641).
void LiveUpdateCameraOrbitInput();
// func_8006E640 (146C.c:2794).
void LiveTrackActionCameraTarget(int tracked_actor_index);
// func_8006EBF8 (146C.c:2965). The original returns an uninitialized
// register its one caller discards; this returns 0.
int LiveUpdateAshleyFollowCamera();
// func_8006EC7C (146C.c:2984).
void LiveUpdateFirstPersonCamera();
// func_8006EF10 (146C.c:3044).
void LiveStorePreferredActionCameraDistance();
// _computeCameraTransition (146C.c:3058).
void LiveComputeFirstPersonCameraTransition(const VECTOR* target_position);
// _stepCameraTransition (146C.c:3121): 1 once the camera arrives.
int LiveStepFirstPersonCameraTransition();
// _initBattleCameraTransition (146C.c:4544).
void LiveBeginFirstPersonView();
// _endBattleCameraTransition (146C.c:4591).
void LiveEndFirstPersonView();
// func_80074580 (146C.c:5165).
void LiveUpdateFirstPersonViewEntry();
// _checkFirstPersonViewExit (146C.c:5179).
void LiveCheckFirstPersonViewExit();
// func_80074744 (146C.c:5222).
void LiveUpdateFirstPersonViewExit();
// func_80074B14 (146C.c:5339).
void LiveUpdateCameraTileConstraint(int actor_index,
                                    const Actors_TileTarget* actor_tile);
// _isLookAtAtDestination (146C.c:6788).
[[nodiscard]] int LiveIsCameraLookAtOnAshley();
// func_80077EC4 (146C.c:6811).
void LiveResetCameraToAshley();
// vs_battle_syncCameraAnglesFromPosition (146C.c:8057): the yaw. A null
// record only updates the view camera's angles.
int LiveSyncCameraAnglesFromPosition(Field_SphericalCamera* output);
// _computeSphericalOffset (146C.c:8092). An output that aliases the base
// reads the normalized direction back as the base, as the original did.
void LiveComputeSphericalCameraPosition(
    VECTOR* out_position, const VECTOR* base_position,
    const Field_SphericalCameraValues* angles);
// _setCameraPositionFromAngles (146C.c:8104).
void LiveSetCameraPositionFromAngles(const Field_SphericalCameraValues* angles);
// _setCameraLookAtFromAngles (146C.c:8110).
void LiveSetCameraLookAtFromAngles(const Field_SphericalCameraValues* angles);
// vs_battle_getCameraPosition (146C.c:8125).
void LiveCameraPosition(VECTOR* out_position);
// vs_battle_setCameraPosition (146C.c:8132).
void LiveSetCameraPosition(const VECTOR* in_position);
// vs_battle_getCameraLookAt (146C.c:8139).
void LiveCameraLookAt(VECTOR* out_look_at);
// vs_battle_setCameraLookAt (146C.c:8146).
void LiveSetCameraLookAt(const VECTOR* in_look_at);
// vs_battle_setCameraRoll (146C.c:8153).
void LiveSetCameraRoll(int roll_angle);
// vs_battle_getCameraRoll (146C.c:8155).
[[nodiscard]] int LiveCameraRoll();
// func_8007BDD8 (146C.c:8573): the new enabled flag.
int LiveSetFirstPersonViewDisabled(int disabled);
// vs_battle_initialiseCameraFromSpherical (146C.c:9071).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveInitializeSphericalCamera(VECTOR* out_position, VECTOR* out_look_at,
                                   int facing, int mode);
// func_8007CE74 (146C.c:9106).
void LiveSetCameraPitchPreset(int requested_pitch);
}  // namespace VS::Field
