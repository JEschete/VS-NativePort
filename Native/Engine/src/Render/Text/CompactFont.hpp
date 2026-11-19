#pragma once
// The compact font's glyph tables, which the battle module image fills.
#include <cstddef>
#include <cstdint>

namespace VS::Render {
inline constexpr std::size_t kCompactGlyphLeftTrimBytes = 264;
inline constexpr std::size_t kCompactGlyphWidthBytes = 92;
inline constexpr std::size_t kCompactGlyphIndexBytes = 60;

// decomp: D_800EB708 (D_800EB708@BATTLE/BATTLE.PRG)
extern std::uint8_t compactGlyphLeftTrim[kCompactGlyphLeftTrimBytes];
// decomp: D_800EB7B4 (D_800EB7B4@BATTLE/BATTLE.PRG)
extern std::uint8_t compactGlyphWidths[kCompactGlyphWidthBytes];
// decomp: D_800EB7D4 (D_800EB7D4@BATTLE/BATTLE.PRG)
extern std::uint8_t compactGlyphIndices[kCompactGlyphIndexBytes];
}  // namespace VS::Render
