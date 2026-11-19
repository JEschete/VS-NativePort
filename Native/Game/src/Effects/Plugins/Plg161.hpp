#pragma once
// PLG161.BIN's module data (src/EFFECT/PLG161.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg161 {
// decomp: D_800FEC68 (PLG161.BIN/F8.c:8; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FEC88 (PLG161.BIN/F8.c:10; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: D_800FEC8C (PLG161.BIN/F8.c:14; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors2;
}  // namespace VS::Effects::Plg161
