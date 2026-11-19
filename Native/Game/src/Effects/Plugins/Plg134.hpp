#pragma once
// PLG134.BIN's module data (src/EFFECT/PLG134.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg134 {
// decomp: D_800FC96C (PLG134.BIN/E0.c:5; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: D_800FC970 (PLG134.BIN/E0.c:7; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg134
