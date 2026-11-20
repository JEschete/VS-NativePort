#pragma once
// The event camera's live state: the field camera override that skips the
// next restore and the camera workspace an event allocates from the high end
// of the heap. ScriptCamera.cpp defines both.

#include <cstdint>

namespace VS::Event {
struct CameraState;

// Nonzero skips the next field camera restore (EventFieldCameraOverride,
// ConsumeFieldCameraOverride).
// decomp: D_800F4BA0 (D_800F4BA0@BATTLE/BATTLE.PRG)
extern std::uint8_t skipNextFieldCameraRestore;
// The event camera workspace (Event/Camera.hpp) InitializeEventCameraEntry
// allocates and the script VM host frees.
// decomp: D_800F4BA4 (D_800F4BA4@BATTLE/BATTLE.PRG)
extern CameraState* cameraWorkspace;
}  // namespace VS::Event
