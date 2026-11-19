#pragma once
// PLG301.BIN's module data (src/EFFECT/PLG301.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg301 {
// decomp: D_800FC524 (PLG301.BIN/110.c:5; particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
// decomp: D_800FC528 (PLG301.BIN/110.c:10; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg301
