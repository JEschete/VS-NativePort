// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Field_CameraPositionPair;
struct Field_CameraTransition;
struct Field_SphericalCamera;
struct Field_SphericalCameraValues;

#include "Render/Render.hpp"

// decomp: _sphericalValues (src/BATTLE/BATTLE.PRG/146C.h:738)
struct Field_SphericalCameraValues {
    int yaw;
    int pitch;
    int distance;
    int mode;
};
static_assert(sizeof(Field_SphericalCameraValues) == 0x10, "Field_SphericalCameraValues must keep its PS1 layout");

// decomp: _sphericalCamera (src/BATTLE/BATTLE.PRG/146C.h:746)
struct Field_SphericalCamera {
    Field_SphericalCameraValues values;
    Field_SphericalCameraValues delta;
    int initialDistance;
    int zoomVelocity;
    int speed;
};
static_assert(sizeof(Field_SphericalCamera) == 0x2C, "Field_SphericalCamera must keep its PS1 layout");

// decomp: func_80077DF0_t (src/BATTLE/BATTLE.PRG/146C.c:211)
struct Field_CameraPositionPair {
    VECTOR position;
    VECTOR lookAt;
};
static_assert(sizeof(Field_CameraPositionPair) == 0x20, "Field_CameraPositionPair must keep its PS1 layout");

// decomp: vs_battle_cameraTransition_t (src/BATTLE/BATTLE.PRG/146C.h:823)
struct Field_CameraTransition {
    VS::Render::Camera fromCameraState;
    Field_SphericalCamera fromSpherical;
    int fromNearClip;
    int fromProjectionDistance;
    int fromFarClip;
    Field_SphericalCamera toSpherical;
    int toNearClip;
    int toProjectionDistance;
    int toFarClip;
    VECTOR targetPosition;
    VECTOR targetLookAt;
    VECTOR direction;
    VECTOR lookAtStep;
    int farClipStep;
    int nearClipStep;
    int projectionDistanceStep;
    int transitionSpeed;
    int transitionFrames;
};
static_assert(sizeof(Field_CameraTransition) == 0xF8, "Field_CameraTransition must keep its PS1 layout");
