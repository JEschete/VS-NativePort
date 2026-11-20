// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Render/Text/CompactFont.hpp"

#include "Legacy/Psx.hpp"

// decomp: D_800EB708 (D_800EB708@BATTLE/BATTLE.PRG)
// contents: module image (ModuleData.cpp)
std::uint8_t
    VS::Render::compactGlyphLeftTrim[VS::Render::kCompactGlyphLeftTrimBytes];

// decomp: D_800EB7B4 (D_800EB7B4@BATTLE/BATTLE.PRG)
// contents: module image (ModuleData.cpp)
std::uint8_t
    VS::Render::compactGlyphWidths[VS::Render::kCompactGlyphWidthBytes];

// decomp: D_800EB7D4 (D_800EB7D4@BATTLE/BATTLE.PRG)
// contents: module image (ModuleData.cpp)
std::uint8_t
    VS::Render::compactGlyphIndices[VS::Render::kCompactGlyphIndexBytes];
