#pragma once
// PLG073.BIN's module data (src/EFFECT/PLG073.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg073 {
// decomp: D_800FEF3C (PLG073.BIN/F8.c:5; closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
// decomp: D_800FEF5C (PLG073.BIN/F8.c:7; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FEF7C (PLG073.BIN/F8.c:9; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
}  // namespace VS::Effects::Plg073
