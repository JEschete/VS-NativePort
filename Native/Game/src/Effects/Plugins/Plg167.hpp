#pragma once
// PLG167.BIN's module data (src/EFFECT/PLG167.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg167 {
// decomp: D_800FD824 (PLG167.BIN/E0.c:5; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: D_800FD844 (PLG167.BIN/E0.c:9; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
}  // namespace VS::Effects::Plg167
