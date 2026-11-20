// The minimap's slide, border pulse and visibility (Minimap.hpp).
#include "Field/Minimap.hpp"

#include <cstdint>

namespace VS::Field {
// decomp: D_800F1BA0 (D_800F1BA0@BATTLE/BATTLE.PRG)
std::int32_t minimapBorderPulsePhase;

// decomp: D_800F1CD4 (D_800F1CD4@BATTLE/BATTLE.PRG)
std::int16_t minimapSlideFrame;

// decomp: D_800F1CD6 (D_800F1CD6@BATTLE/BATTLE.PRG)
std::uint8_t isMinimapVisible;
}  // namespace VS::Field
