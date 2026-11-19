#pragma once
// PLG135.BIN's module data (src/EFFECT/PLG135.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg135 {
// decomp: D_800FC96C (PLG135.BIN/E0.c:5; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: D_800FC98C (PLG135.BIN/E0.c:7; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
}  // namespace VS::Effects::Plg135
