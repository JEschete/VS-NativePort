#pragma once
// PLG169.BIN's module data (src/EFFECT/PLG169.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg169 {
// decomp: D_800FD590 (PLG169.BIN/E0.c:5; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: D_800FD594 (PLG169.BIN/E0.c:7; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FD5B4 (PLG169.BIN/E0.c:9; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg169
