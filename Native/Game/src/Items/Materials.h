// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


extern u_char Items_weaponMaterialPaletteIndices[3432];

namespace VS::Items {
// decomp: vs_main_materialModifiers (main.c:7950): each material's
// 30-byte modifier record.
extern char materialModifiers[8][30];
}  // namespace VS::Items
