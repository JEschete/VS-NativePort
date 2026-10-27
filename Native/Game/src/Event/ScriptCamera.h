// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_SCRIPTCAMERA_H_TYPES
#define VS_EVENT_SCRIPTCAMERA_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EVENT_SCRIPTCAMERA_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EVENT_SCRIPTCAMERA_H_DECLS)
#define VS_EVENT_SCRIPTCAMERA_H_DECLS
#include "Event/Camera.h"
#include "Event/Screen.h"
#include "Render/FramebufferEffects.h"

extern CVECTOR Event_kDefaultScreenTint;
extern Render_FramebufferEffectTranslation Event_kDefaultScreenTransform;
extern u_char Event_fieldCameraReturnCommands[276];
extern short Event_kOrbitReturnHeightOffsets[134];
extern u_char Event_kOrbitReturnFirstCommand[4];
extern u_char Event_kOrbitReturnSecondCommand[3];
extern char Event_skipNextFieldCameraRestore;
extern Event_CameraState* Event_camera;
intptr_t Event_CommandStopCameraMotion(u_char* instruction, short thread_index);
intptr_t Event_CommandSetCameraFollowing(u_char* instruction, short thread_index);
intptr_t Event_CommandSetCameraOriginActor(u_char* instruction, short thread_index);
intptr_t Event_CommandRestoreFieldCamera(u_char* instruction, short thread_index);
intptr_t Event_CommandWaitCameraTracks(u_char* instruction, short thread_index);
intptr_t Event_CommandSetScreenCapture(u_char* instruction, short thread_index);
void Event_InitializeEventCamera(void);
void Event_FreeEventCamera(void);
void Event_SetEventFieldCameraOverride(int arg0);
void Event_SetEventCameraOriginActor(u_short arg0);
void Event_UpdateEventCameraOrigin(void);
void Event_ApplyEventCameraState(void);
void Event_RestoreEventFieldCamera(short arg0, int arg1);
intptr_t Event_CommandCameraMotion(u_char* script, short unused);
void Event_UpdateEventCameraTracks(void);
void Event_UpdateEventCameraTrack(Event_CameraTrack* c, Event_CameraTrack* other);
void Event_AdvanceTerrainCameraTrack(Event_CameraTrack* c);
int Event_EventCameraGroundClearance(MATRIX* arg0);
intptr_t Event_CommandTweenCameraRoll(u_char* instruction, short thread_index);
void Event_UpdateEventAngleTween(Event_AngleTween* arg0);
int Event_EventCameraCompletionMask(void);
intptr_t Event_CommandScreenTransform(u_char* script, short thread_index);
void Event_UpdateEventScreenTransforms(void);
int Event_UpdateEventTripleTween(Event_TripleTween* arg0);
intptr_t Event_CommandCameraLensOrShake(u_char* script, short thread_index);
void Event_UpdateEventLensTween(Event_LensTween* arg0);
int Event_UpdateEventShakeOscillator(Event_ShakeOscillator* arg0);
void Event_UpdateEventCameraShake(void);
void Event_SetEventCameraFollowing(int arg0, int arg1);
void Event_StopEventCameraMotion(void);
void Event_StartEventCameraOrbitReturn(char arg0);
void Event_ScaleEventCameraTrackDuration(Event_CameraTrack* arg0);
void Event_ScaleEventCameraDuration(short arg0);
VECTOR* Event_ReadScriptFixedVector(VECTOR* arg0, u_char* arg1);

#endif // VS_EVENT_SCRIPTCAMERA_H_DECLS
