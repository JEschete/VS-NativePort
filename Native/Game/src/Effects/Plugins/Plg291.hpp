#pragma once
// PLG291.BIN's module data (src/EFFECT/PLG291.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg291 {
// decomp: D_800FD4D8 (PLG291.BIN/F8.c:5; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: D_800FD4DC (PLG291.BIN/F8.c:10; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg291
