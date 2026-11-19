// PLG087.BIN's module data (src/EFFECT/PLG087.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/Plg087.hpp"

namespace VS::Effects::Plg087 {
// decomp: D_800FF75C (PLG087.BIN/1F0.c:6; sphericalSurfaceVertices.h:21)
plugins::SurfaceProfile surfaceVertices;
// decomp: D_800FF9DC (PLG087.BIN/1F0.c:9; closedRings.h:58)
plugins::RingPalette closedRingColorFactors;
// decomp: D_800FF9FC (PLG087.BIN/1F0.c:13; trailEffect.h:70)
std::int32_t trailEmissionCounter;
// decomp: D_800FFA00 (PLG087.BIN/1F0.c:16; particleSetup.h:35)
std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg087
