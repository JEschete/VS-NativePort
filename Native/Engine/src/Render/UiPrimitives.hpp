#pragma once
// The stat gauges' colors, which the battle module image fills.
#include <cstddef>

namespace VS::Render {
inline constexpr std::size_t kStatGaugeColorCount = 4;

// decomp: D_800EBC04 (D_800EBC04@BATTLE/BATTLE.PRG)
extern int kStatGaugeColors[kStatGaugeColorCount];
}  // namespace VS::Render
