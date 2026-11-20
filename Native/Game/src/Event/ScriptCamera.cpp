// The event camera's live state (ScriptCamera.hpp).
#include "Event/ScriptCamera.hpp"

#include <cstdint>

namespace VS::Event {
// decomp: D_800F4BA0 (D_800F4BA0@BATTLE/BATTLE.PRG)
std::uint8_t skipNextFieldCameraRestore;

// decomp: D_800F4BA4 (D_800F4BA4@BATTLE/BATTLE.PRG)
CameraState* cameraWorkspace;
}  // namespace VS::Event
