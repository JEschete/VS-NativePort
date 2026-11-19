// Storage for the material tables (Materials.hpp).
#include "Items/Materials.hpp"

namespace VS::Items {
// decomp: _weaponMaterialPaletteMap
// (_weaponMaterialPaletteMap@BATTLE/BATTLE.PRG)
std::uint8_t weaponMaterialPaletteIndices[kWeaponMaterialPaletteBytes];

// decomp: vs_main_materialModifiers (vs_main_materialModifiers@SLUS_010.40)
std::array<MaterialModifierRecord, kMaterialCount> materialModifiers{};
static_assert(sizeof(materialModifiers) ==
              kMaterialCount * kMaterialModifierBytes);
}  // namespace VS::Items
