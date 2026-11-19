#pragma once
// PLG082.BIN's module data (src/EFFECT/PLG082.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg082 {
// decomp: D_800FD088 (PLG082.BIN/128.c:5; particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
// decomp: D_800FD08C (PLG082.BIN/128.c:11; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg082
