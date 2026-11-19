// PLG263.BIN's module data (src/EFFECT/PLG263.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/Plg263.hpp"

namespace VS::Effects::Plg263 {
// decomp: D_800FDCB0 (PLG263.BIN/110.c:5; sphericalSurfaceVertices.h:21)
plugins::SurfaceProfile surfaceVertices;
// decomp: D_800FDF30 (PLG263.BIN/110.c:7; rings.h:20)
plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FDF50 (PLG263.BIN/110.c:10; closedRings.h:58)
plugins::RingPaletteBytes ringColorFactors2;
// decomp: D_800FDF70 (PLG263.BIN/110.c:12; particleSetup.h:35)
std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg263
