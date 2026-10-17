// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_CAMERA_H_TYPES
#define VS_EVENT_CAMERA_H_TYPES
#include "Legacy/Psx.h"

typedef struct Event_AngleTween Event_AngleTween;
typedef struct Event_CameraMotionScratch Event_CameraMotionScratch;
typedef struct Event_CameraShakeScratch Event_CameraShakeScratch;
typedef struct Event_CameraState Event_CameraState;
typedef struct Event_CameraTrack Event_CameraTrack;
typedef struct Event_CameraTrackScratch Event_CameraTrackScratch;
typedef struct Event_LensTween Event_LensTween;
typedef struct Event_ShakeOscillator Event_ShakeOscillator;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_EVENT_CAMERA_H_SET_TYPES_ONLY
#endif
#include "Event/Screen.h"
#ifdef VS_EVENT_CAMERA_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_EVENT_CAMERA_H_SET_TYPES_ONLY
#endif

// decomp: D_800F4BA4_t2 (src/BATTLE/BATTLE.PRG/4A0A8.c:42)
typedef struct Event_CameraTrack {
    u_char motionMode;
    u_char easingPrimary;
    u_char easingSecondary;
    u_char easingTertiary;
    u_char elapsedFrames;
    u_char durationFrames;
    u_char followRequest;
    u_char isFollowing;
    int motionScalar;
    int motionScalarDelta;
    short orbitAngle;
    short orbitAngleDelta;
    VECTOR startPosition;
    VECTOR destination;
    VECTOR displacement;
    VECTOR heightOffset;
    VECTOR followDelta;
    VECTOR followPosition;
    VECTOR previousCameraPosition;
    VECTOR computedPosition;
    VECTOR currentPosition;
    VECTOR partnerPosition;
} Event_CameraTrack;

// decomp: func_800BD57C_t (src/BATTLE/BATTLE.PRG/4A0A8.c:52)
typedef struct Event_AngleTween {
    char hasChanged;
    char easingMode;
    short angle;
    short startAngle;
    short deltaAngle;
    short elapsedFrames;
    short durationFrames;
} Event_AngleTween;

// decomp: func_800BDF6C_t (src/BATTLE/BATTLE.PRG/4A0A8.c:66)
typedef struct Event_ShakeOscillator {
    union {
        u_char bytes[2];
        short count;
    } envelopeShape;
    short displacement;
    short cycles;
    short amplitude;
    char elapsedFrames;
    char durationFrames;
    char axisMask;
    char reserved0B;
} Event_ShakeOscillator;

// decomp: func_800BDF6C_unk180_t (src/BATTLE/BATTLE.PRG/4A0A8.c:78)
typedef struct Event_LensTween {
    char hasChanged;
    char easingMode;
    short value;
    short startValue;
    short deltaValue;
    char elapsedFrames;
    char durationFrames;
    char reserved0A;
    char reserved0B;
} Event_LensTween;

// decomp: D_800F4BA4_t (src/BATTLE/BATTLE.PRG/4A0A8.c:127)
typedef struct Event_CameraState {
    Event_CameraTrack lookAt;
    Event_CameraTrack eye;
    Event_AngleTween cameraRoll;
    Event_AngleTween capturedScreenRoll;
    Event_LensTween projectionDistance;
    Event_LensTween nearClip;
    Event_LensTween farClip;
    Event_ShakeOscillator shakeOscillatorA;
    Event_ShakeOscillator shakeOscillatorB;
    short reserved1BC;
    short shakeEyeOffsetX;
    short shakeEyeOffsetY;
    short shakeEyeOffsetZ;
    short reserved1C4;
    short shakeLookAtOffsetX;
    short shakeLookAtOffsetY;
    short shakeLookAtOffsetZ;
    int actorQueryScratch[7];
    u_short originActorId;
    u_short reserved1EA;
    VECTOR originOffset;
    Event_TripleTween screenScale;
    Event_TripleTween screenTint;
    Event_TripleTween screenTranslation;
    Event_TripleTween screenEffectTween;
    VECTOR lastTerrainPosition;
} Event_CameraState;

// decomp: cameraMotionScratch (src/BATTLE/BATTLE.PRG/4A0A8.c:221)
typedef struct Event_CameraMotionScratch {
    int distance;
    int steps;
    char pad08[8];
    VECTOR delta;
    VECTOR candidate;
    VECTOR direction;
    char pad40[0x30];
    MATRIX matrix;
} Event_CameraMotionScratch;

// decomp: cameraScratch (src/BATTLE/BATTLE.PRG/4A0A8.c:210)
typedef struct Event_CameraTrackScratch {
    int value;
    int distance;
    char pad08[8];
    VECTOR currentOffset;
    VECTOR destinationOffset;
    VECTOR workVector;
    char pad40[16];
    SVECTOR rotation;
    SVECTOR actorRotation;
    char pad60[16];
    MATRIX matrix;
} Event_CameraTrackScratch;

// decomp: cameraShakeScratch (src/BATTLE/BATTLE.PRG/4A0A8.c:228)
typedef struct Event_CameraShakeScratch {
    char pad00[0x50];
    SVECTOR eyeShake;
    SVECTOR lookAtShake;
    SVECTOR axisShake;
} Event_CameraShakeScratch;

#endif // VS_EVENT_CAMERA_H_TYPES
