#pragma once
// The minimap's slide, border pulse and visibility. Minimap.cpp defines them.

#include <cstdint>

namespace VS::Field {
// The minimap border's pulse phase.
// decomp: D_800F1BA0 (D_800F1BA0@BATTLE/BATTLE.PRG,
// src/BATTLE/BATTLE.PRG/146C.c:356)
extern std::int32_t minimapBorderPulsePhase;
// The minimap's slide in and out.
// decomp: D_800F1CD4 (D_800F1CD4@BATTLE/BATTLE.PRG,
// src/BATTLE/BATTLE.PRG/146C.h:53)
extern std::int16_t minimapSlideFrame;
// Nonzero while the minimap shows (Field/Map.hpp SetMinimapVisible).
// decomp: D_800F1CD6 (D_800F1CD6@BATTLE/BATTLE.PRG,
// src/BATTLE/BATTLE.PRG/146C.h:54)
extern std::uint8_t isMinimapVisible;
}  // namespace VS::Field
