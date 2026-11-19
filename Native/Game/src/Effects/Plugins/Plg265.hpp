#pragma once
// PLG265.BIN's module data (src/EFFECT/PLG265.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg265 {
// decomp: D_800FFD98 (PLG265.BIN/2E8.c:5; closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
// decomp: D_800FFDB8 (PLG265.BIN/2E8.c:8; particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
// decomp: D_800FFDBC (PLG265.BIN/2E8.c:13; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: D_800FFDC0 (PLG265.BIN/2E8.c:15; particleSetup.h:35)
extern std::int32_t particleEmissionCounter2;
}  // namespace VS::Effects::Plg265
