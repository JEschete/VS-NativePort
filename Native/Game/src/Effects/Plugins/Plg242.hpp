#pragma once
// PLG242.BIN's module data (src/EFFECT/PLG242.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg242 {
// decomp: D_800FD460 (PLG242.BIN/F8.c:5; closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
// decomp: D_800FD480 (PLG242.BIN/F8.c:9; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
}  // namespace VS::Effects::Plg242
