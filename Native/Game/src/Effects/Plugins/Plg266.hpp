#pragma once
// PLG266.BIN's module data (src/EFFECT/PLG266.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

namespace VS::Effects::Plg266 {
// decomp: D_800FEEA0 (PLG266.BIN/2E8.c:5; particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
// decomp: D_800FEEA4 (PLG266.BIN/2E8.c:10; particleSetup.h:35)
extern std::int32_t particleEmissionCounter2;
// decomp: D_800FEEA8 (PLG266.BIN/2E8.c:15; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
}  // namespace VS::Effects::Plg266
