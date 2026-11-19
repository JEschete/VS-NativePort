#pragma once
// PLG065.BIN's module data (src/EFFECT/PLG065.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg065 {
// decomp: D_800FD934 (PLG065.BIN/F8.c:7; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: D_800FD954 (PLG065.BIN/F8.c:9; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FD974 (PLG065.BIN/F8.c:12; particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg065
