// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Event/Camera.hpp"

#include "Legacy/Psx.h"
#include "Event/ScriptCamera.h"
#include "Render/FramebufferEffects.hpp"

// decomp: D_800E9C08 (D_800E9C08@BATTLE/BATTLE.PRG)
CVECTOR Event_kDefaultScreenTint; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9C0C (D_800E9C0C@BATTLE/BATTLE.PRG)
VS::Render::FramebufferEffectTranslation Event_kDefaultScreenTransform; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9C1C (D_800E9C1C@BATTLE/BATTLE.PRG)
u_char Event_fieldCameraReturnCommands[276]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9C24 (D_800E9C24@BATTLE/BATTLE.PRG)
short Event_kOrbitReturnHeightOffsets[134]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9C28 (D_800E9C28@BATTLE/BATTLE.PRG)
u_char Event_kOrbitReturnFirstCommand[4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9C2C (D_800E9C2C@BATTLE/BATTLE.PRG)
u_char Event_kOrbitReturnSecondCommand[3]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F4BA0 (D_800F4BA0@BATTLE/BATTLE.PRG)
char Event_skipNextFieldCameraRestore;

// decomp: D_800F4BA4 (D_800F4BA4@BATTLE/BATTLE.PRG)
VS::Event::CameraState* Event_camera;

