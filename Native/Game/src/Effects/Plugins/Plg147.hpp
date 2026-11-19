#pragma once
// PLG147.BIN's module data (src/EFFECT/PLG147.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg147 {
// decomp: D_800FBB10 (PLG147.BIN/F8.c:5; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FBB30 (PLG147.BIN/F8.c:8; particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg147
