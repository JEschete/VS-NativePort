#pragma once
// The actor texture slot tables: each slot's VRAM origin (a halfword column
// and a halfword page) and its row offset. Textures.cpp defines them; their
// contents come from the BATTLE.PRG module image.

#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Actors {
inline constexpr std::size_t kTextureSlotOriginBytes = 152;
inline constexpr std::size_t kTextureSlotRowOffsetBytes = 56;
using TextureSlotOrigins = std::array<std::uint8_t, kTextureSlotOriginBytes>;
using TextureSlotRows = std::array<std::uint8_t, kTextureSlotRowOffsetBytes>;

// decomp: D_800E8F30 (D_800E8F30@BATTLE/BATTLE.PRG)
extern TextureSlotOrigins textureSlotOrigins;
// decomp: D_800E8F90 (D_800E8F90@BATTLE/BATTLE.PRG)
extern TextureSlotRows textureSlotRowOffsets;
}  // namespace VS::Actors
