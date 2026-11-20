// The actor texture slot tables (Textures.hpp).
#include "Actors/Textures.hpp"

namespace VS::Actors {
// decomp: D_800E8F30 (D_800E8F30@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<std::uint8_t, kTextureSlotOriginBytes> textureSlotOrigins;
// decomp: D_800E8F90 (D_800E8F90@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<std::uint8_t, kTextureSlotRowOffsetBytes> textureSlotRowOffsets;
}  // namespace VS::Actors
