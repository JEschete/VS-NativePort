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

#endif // VS_EVENT_SCRIPTCAMERA_H_DECLS
