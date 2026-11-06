#pragma once
// Hand-maintained (tools/port/owned.csv).
// The field camera globals as the copied battle flow, scripts, sphere and
// snapshot read and write them. CameraLive.cpp defines them and mirrors them
// into the typed FieldCameraState (Field/Camera.hpp) around each call.

#include "Field/Camera.h"
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

// decomp: _firstPersonViewEnabled (BATTLE/BATTLE.PRG 146C.c:648): 1 while
// Select may enter the first-person view.
extern int Field_isFirstPersonViewEnabled;
// decomp: vs_battle_cameraTransition (BATTLE/BATTLE.PRG 146C.h:1119): the
// first-person transition's high-end heap record, null outside the view.
extern Field_CameraTransition* Field_cameraTransition;
// decomp: D_800F19C8 (BATTLE/BATTLE.PRG 146C.c:666): 1 while the
// first-person view hides the room's occluding geometry.
extern int Field_roomGeometryDrawMode;
// decomp: vs_battle_cameraCurrentSpherical (BATTLE/BATTLE.PRG 146C.h:1096):
// the orbit's angles, quarter turn, zoom and tracking speed.
extern Field_SphericalCamera Field_sphericalCamera;

#ifdef __cplusplus
}
#endif
