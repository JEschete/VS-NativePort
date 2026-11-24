#pragma once
// The weapon palette map of the BATTLE.PRG module image.
#include <cstddef>
#include <cstdint>

namespace VS::Items {
inline constexpr std::size_t kWeaponMaterialPaletteBytes = 3432;

// decomp: weaponMaterialPaletteMap (src/BATTLE/BATTLE.PRG/30DB0.c:80; was
// _weaponMaterialPaletteMap): the palette index of each weapon and material.
extern std::uint8_t weaponMaterialPaletteIndices[kWeaponMaterialPaletteBytes];

}  // namespace VS::Items
