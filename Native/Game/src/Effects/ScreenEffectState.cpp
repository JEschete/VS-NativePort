#include "Effects/ScreenEffectState.hpp"

namespace VS::Effects {

// decomp: D_800EB9B4 (src/GIM/SCREFF2.PRG/0.h:22)
LiveScreenEffectOverlay* screenEffectOverlay;

// decomp: D_800F4ED8 (src/BATTLE/BATTLE.PRG/5BF94.c:425; reset 5BF94.c:3046).
// Original short[8] aliases a 16-byte overlay; native pointer ownership needs
// the complete native type, not the original byte capacity.
LiveScreenEffectOverlay screenEffectDefaults{};

}  // namespace VS::Effects
