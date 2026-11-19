// PLG146.BIN's module data (src/EFFECT/PLG146.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/Plg146.hpp"

namespace VS::Effects::Plg146 {
// decomp: D_800FF87C (PLG146.BIN/1D8.c:5; closedRings.h:58)
plugins::RingPalette tiledRingColorFactors;
// decomp: D_800FF89C (PLG146.BIN/1D8.c:7; sphericalSurfaceVertices.h:21)
plugins::SurfaceProfile mirrorSurfaceVertices;
// decomp: D_800FFB1C (PLG146.BIN/1D8.c:10; particleSetup.h:35)
std::int32_t particleEmissionCounter;
// decomp: D_800FFB20 (PLG146.BIN/1D8.c:15; rings.h:20)
plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FFB40 (PLG146.BIN/1D8.c:17; trailEffect.h:70)
std::int32_t trailEmissionCounter;
}  // namespace VS::Effects::Plg146
