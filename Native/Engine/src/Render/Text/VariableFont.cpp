// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Render/Text/VariableFont.hpp"

#include "Legacy/Psx.hpp"

// decomp: vs_battle_characterWidths
// (vs_battle_characterWidths@BATTLE/BATTLE.PRG)
// contents: module image (ModuleData.cpp)
std::uint8_t
    VS::Render::variableFontGlyphWidths[VS::Render::kVariableFontGlyphCount];

// decomp: D_800EB98C (D_800EB98C@BATTLE/BATTLE.PRG)
// contents: module image (ModuleData.cpp)
std::uint16_t
    VS::Render::kGlyphPixelExpansion[VS::Render::kGlyphPixelExpansionEntries];
