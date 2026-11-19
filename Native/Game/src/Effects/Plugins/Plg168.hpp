#pragma once
// PLG168.BIN's module data (src/EFFECT/PLG168.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

namespace VS::Effects::Plg168 {
// decomp: D_800FDC54 (PLG168.BIN/1F0.c:5; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: D_800FDC58 (PLG168.BIN/1F0.c:8; particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg168
