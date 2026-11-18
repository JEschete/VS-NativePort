#include "Effects/ScreenEffectState.hpp"

#include "Effects/ScreenEffects.h"
#include "Legacy/Psx.h"

// decomp: BATTLE.PRG D_800F4ED8 (5BF94.c:3481/3919).
// Original short[8] aliases a 16-byte overlay; native pointer ownership needs
// the complete native type, not the original byte capacity.
Effects_ScreenEffectOverlay VS::Effects::screenEffectDefaults{};
