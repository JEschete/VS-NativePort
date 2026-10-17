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

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_CAMERA_H_DECLS)
#define VS_FIELD_CAMERA_H_DECLS
#include "Actors/Location.h"

extern int Field_isFirstPersonViewEnabled;
extern Field_CameraTransition* Field_cameraTransition;
extern int Field_roomGeometryDrawMode;
extern Field_SphericalCamera Field_sphericalCamera;
void Field_UpdateCameraFacingState(void);
int Field_AddCameraStepWithFallback(int base, int a, int b);
int Field_AddClampedCameraStep(int base, int a, int b);
void Field_ClampCameraLookAtToZone(VECTOR* inPos, VECTOR* outPos);
int Field_ComputeCameraTrackingStep(SVECTOR* targetPos, VECTOR* outStep,
    VECTOR* outDisplacement, int clampToZone, int maxSpeed);
int Field_StepCameraLookAtTowardTarget(SVECTOR* targetPos, int transformFlag, int maxSpeed);
void Field_TrackCameraLookAtWithinDeadZone(VECTOR* arg0);
void Field_UpdateCameraOrbitInput(void);
void Field_TrackActionCameraTarget(int arg0);
int Field_UpdateAshleyFollowCamera(void);
void Field_UpdateFirstPersonCamera(void);
void Field_StorePreferredActionCameraDistance(void);
void Field_ComputeFirstPersonCameraTransition(VECTOR* targetPosition);
int Field_StepFirstPersonCameraTransition(void);
void Field_BeginFirstPersonView(void);
void Field_EndFirstPersonView(void);
void Field_UpdateFirstPersonViewEntry(void);
void Field_CheckFirstPersonViewExit(void);
void Field_UpdateFirstPersonViewExit(void);
void Field_UpdateCameraTileConstraint(int arg0, Actors_TileTarget* arg1);
int Field_IsCameraLookAtOnAshley(void);
void Field_ResetCameraToAshley(void);
int Field_SyncCameraAnglesFromPosition(Field_SphericalCamera* arg0);
void Field_ComputeSphericalCameraPosition(
    VECTOR* outPosition, VECTOR* basePosition, Field_SphericalCameraValues* angles);
void Field_SetCameraPositionFromAngles(Field_SphericalCameraValues* spherical);
void Field_SetCameraLookAtFromAngles(Field_SphericalCameraValues* spherical);
void Field_CameraPosition(VECTOR* outPosition);
void Field_SetCameraPosition(VECTOR* inPosition);
void Field_CameraLookAt(VECTOR* outLookAt);
void Field_SetCameraLookAt(VECTOR* inLookAt);
void Field_SetCameraRoll(int arg0);
int Field_CameraRoll(void);
int Field_SetFirstPersonViewDisabled(int arg0);
void Field_InitializeSphericalCamera(
    VECTOR* outputPos, VECTOR* outputLookAt, int facing, int mode);
void Field_SetCameraPitchPreset(int arg0);

#endif // VS_FIELD_CAMERA_H_DECLS
