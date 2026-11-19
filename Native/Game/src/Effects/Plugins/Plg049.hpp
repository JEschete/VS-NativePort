#pragma once
// PLG049.BIN's module data (src/EFFECT/PLG049.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg049 {
// decomp: D_800FE3D4 (PLG049.BIN/E0.c:8; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: D_800FE3D8 (PLG049.BIN/E0.c:13; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FE3F8 (PLG049.BIN/E0.c:18; closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
}  // namespace VS::Effects::Plg049
