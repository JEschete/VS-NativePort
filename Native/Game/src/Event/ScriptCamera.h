// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


#include "Render/FramebufferEffects.h"

// The workspace type is defined in Event/Camera.hpp.
namespace VS::Event {
struct CameraState;
}

extern CVECTOR Event_kDefaultScreenTint;
extern Render_FramebufferEffectTranslation Event_kDefaultScreenTransform;
extern u_char Event_fieldCameraReturnCommands[276];
extern short Event_kOrbitReturnHeightOffsets[134];
extern u_char Event_kOrbitReturnFirstCommand[4];
extern u_char Event_kOrbitReturnSecondCommand[3];
extern char Event_skipNextFieldCameraRestore;
extern VS::Event::CameraState* Event_camera;
