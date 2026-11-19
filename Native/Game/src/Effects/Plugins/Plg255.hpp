#pragma once
// PLG255.BIN's module data (src/EFFECT/PLG255.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

namespace VS::Effects::Plg255 {
// decomp: D_800FD0EC (PLG255.BIN/1D8.c:5; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: D_800FD0F0 (PLG255.BIN/1D8.c:8; particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg255
