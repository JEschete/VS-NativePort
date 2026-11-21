#pragma once
// Material tables: the weapon palette map of the BATTLE.PRG module image and
// the modifier records of SLUS_010.40 (ModuleData.cpp fills both).
#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Items {
inline constexpr std::size_t kWeaponMaterialPaletteBytes = 3432;
inline constexpr std::size_t kMaterialCount = 8;
inline constexpr std::size_t kMaterialModifierBytes = 30;

// decomp: weaponMaterialPaletteMap (src/BATTLE/BATTLE.PRG/30DB0.c:80; was
// _weaponMaterialPaletteMap): the palette index of each weapon and material.
extern std::uint8_t weaponMaterialPaletteIndices[kWeaponMaterialPaletteBytes];

// decomp: vs_main_materialModifiers (src/SLUS_010.40/main.c:350): each
// material's modifier record, kept as raw bytes. Its strength, intelligence
// and agility modifiers are the signed bytes at offsets 24, 26 and 28.
using MaterialModifierRecord = std::array<std::uint8_t, kMaterialModifierBytes>;
extern std::array<MaterialModifierRecord, kMaterialCount> materialModifiers;
}  // namespace VS::Items
