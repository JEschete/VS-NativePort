#pragma once
// PLG148.BIN's module data (src/EFFECT/PLG148.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg148 {
// decomp: D_800FC8F8 (PLG148.BIN/E0.c:5; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: D_800FC8FC (PLG148.BIN/E0.c:7; closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
}  // namespace VS::Effects::Plg148
