// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_CAMERA_H_TYPES
#define VS_FIELD_CAMERA_H_TYPES
#include "Legacy/Psx.h"

typedef struct Field_CameraPositionPair Field_CameraPositionPair;
typedef struct Field_CameraTransition Field_CameraTransition;
typedef struct Field_SphericalCamera Field_SphericalCamera;
typedef struct Field_SphericalCameraValues Field_SphericalCameraValues;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_FIELD_CAMERA_H_SET_TYPES_ONLY
#endif
#include "Render/Render.h"
#ifdef VS_FIELD_CAMERA_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_FIELD_CAMERA_H_SET_TYPES_ONLY
#endif

// decomp: _sphericalValues (src/BATTLE/BATTLE.PRG/146C.h:738)
typedef struct Field_SphericalCameraValues {
    int yaw;
    int pitch;
    int distance;
    int mode;
} Field_SphericalCameraValues;
_Static_assert(sizeof(Field_SphericalCameraValues) == 0x10, "Field_SphericalCameraValues must keep its PS1 layout");

// decomp: _sphericalCamera (src/BATTLE/BATTLE.PRG/146C.h:746)
typedef struct Field_SphericalCamera {
    Field_SphericalCameraValues values;
    Field_SphericalCameraValues delta;
    int initialDistance;
    int zoomVelocity;
    int speed;
} Field_SphericalCamera;
_Static_assert(sizeof(Field_SphericalCamera) == 0x2C, "Field_SphericalCamera must keep its PS1 layout");

// decomp: func_80077DF0_t (src/BATTLE/BATTLE.PRG/146C.c:211)
typedef struct Field_CameraPositionPair {
    VECTOR position;
    VECTOR lookAt;
} Field_CameraPositionPair;
_Static_assert(sizeof(Field_CameraPositionPair) == 0x20, "Field_CameraPositionPair must keep its PS1 layout");

// decomp: vs_battle_cameraTransition_t (src/BATTLE/BATTLE.PRG/146C.h:823)
typedef struct Field_CameraTransition {
    Render_Camera fromCameraState;
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
} Field_CameraTransition;
_Static_assert(sizeof(Field_CameraTransition) == 0xF8, "Field_CameraTransition must keep its PS1 layout");

#endif // VS_FIELD_CAMERA_H_TYPES
