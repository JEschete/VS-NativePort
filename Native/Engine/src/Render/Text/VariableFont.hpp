#pragma once
// The variable-width font's glyph widths and the pixel expansion table, which
// the battle module image fills. The original toolchain's plain char was
// unsigned, so the widths are std::uint8_t.
#include <cstddef>
#include <cstdint>

#include "Render/GameFont.hpp"

namespace VS::Render {
inline constexpr std::size_t kGlyphPixelExpansionEntries = 16;

// decomp: vs_battle_characterWidths
// (vs_battle_characterWidths@BATTLE/BATTLE.PRG)
extern std::uint8_t variableFontGlyphWidths[kVariableFontGlyphCount];
// decomp: D_800EB98C (D_800EB98C@BATTLE/BATTLE.PRG)
extern std::uint16_t kGlyphPixelExpansion[kGlyphPixelExpansionEntries];
}  // namespace VS::Render
