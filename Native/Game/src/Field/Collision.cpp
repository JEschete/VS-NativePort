// The collision probe, the platform contact depth and the tile surface table
// (Collision.hpp).
#include "Field/Collision.hpp"

using VS::Core::Gte::SVECTOR;

namespace VS::Field {
// decomp: D_800E9278 (D_800E9278@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<std::uint8_t, kTileSurfaceIdBytes> tileSurfaceIds;
// decomp: D_800F49F9 (D_800F49F9@BATTLE/BATTLE.PRG)
std::uint8_t collisionProbeDirection;
// decomp: D_800F4B00 (D_800F4B00@BATTLE/BATTLE.PRG)
std::int16_t platformContactSearchDepth;
// decomp: D_800F4B08 (D_800F4B08@BATTLE/BATTLE.PRG)
SVECTOR collisionProbePosition;
}  // namespace VS::Field
