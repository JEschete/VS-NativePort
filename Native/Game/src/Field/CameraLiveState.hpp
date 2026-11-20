#pragma once
// Hand-maintained (tools/port/owned.csv).
// The field camera globals as the battle flow, scripts, sphere and snapshot
// read and write them. CameraLive.cpp defines them and mirrors them into the
// typed FieldCameraState (Field/Camera.hpp) around each call.

#include <cstdint>

#include "Field/Camera.hpp"
#include "Legacy/Psx.hpp"
#include "Render/Render.hpp"

namespace VS::Field {

// decomp: vs_battle_cameraTransition_t (BATTLE/BATTLE.PRG 146C.h:823): the
// first-person transition as its high-end heap record holds it, VECTOR pads
// included; FirstPersonTransition (Field/Camera.hpp) is its typed value.
struct CameraTransitionRecord {
  Render::Camera from_camera_state;
  SphericalCamera from_spherical;
  std::int32_t from_near_clip;
  std::int32_t from_projection_distance;
  std::int32_t from_far_clip;
  SphericalCamera to_spherical;
  std::int32_t to_near_clip;
  std::int32_t to_projection_distance;
  std::int32_t to_far_clip;
  VS::Core::Gte::VECTOR target_position;
  VS::Core::Gte::VECTOR target_look_at;
  VS::Core::Gte::VECTOR direction;
  VS::Core::Gte::VECTOR look_at_step;
  std::int32_t far_clip_step;
  std::int32_t near_clip_step;
  std::int32_t projection_distance_step;
  std::int32_t transition_speed;
  std::int32_t transition_frames;
};

// decomp: _firstPersonViewEnabled (BATTLE/BATTLE.PRG 146C.c:648): 1 while
// Select may enter the first-person view.
extern int isFirstPersonViewEnabled;
// decomp: vs_battle_cameraTransition (BATTLE/BATTLE.PRG 146C.h:1119): the
// first-person transition's high-end heap record, null outside the view.
extern CameraTransitionRecord* cameraTransition;
// decomp: D_800F19C8 (BATTLE/BATTLE.PRG 146C.c:666): 1 while the
// first-person view hides the room's occluding geometry.
extern int roomGeometryDrawMode;
// decomp: vs_battle_cameraCurrentSpherical (BATTLE/BATTLE.PRG 146C.h:1096):
// the orbit's angles, quarter turn, zoom and tracking speed.
extern SphericalCamera sphericalCamera;

}  // namespace VS::Field
