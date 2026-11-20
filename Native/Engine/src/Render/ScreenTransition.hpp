#pragma once
// The legacy packets of the battle screen transition: a wipe, its gradient
// edge and a fade, one set for each framebuffer.
#include <cstddef>

#include "Legacy/Render/LibGpu.hpp"
#include "Render/DisplayControl.hpp"

namespace VS::Render {
// decomp: vs_battle_screenTransitionWipePoly
// (src/BATTLE/BATTLE.PRG/146C.h:802)
struct LegacyTransitionWipePacket {
  VS::Legacy::POLY_F4 covered_quad;
  VS::Legacy::POLY_F4 edge_quad;
};

// decomp: vs_battle_screenTransitionDrMode
// (vs_battle_screenTransitionDrMode@BATTLE/BATTLE.PRG)
extern VS::Legacy::DR_MODE legacyTransitionDrawModes[kFrameBufferCount];
// decomp: vs_battle_screenTransitionWipePolyF4
// (vs_battle_screenTransitionWipePolyF4@BATTLE/BATTLE.PRG)
// One wipe packet per frame buffer.
using LegacyTransitionWipes = LegacyTransitionWipePacket[kFrameBufferCount];
extern LegacyTransitionWipes legacyTransitionWipePackets;
// decomp: vs_battle_screenTransitionEdgePolyG4
// (vs_battle_screenTransitionEdgePolyG4@BATTLE/BATTLE.PRG)
extern VS::Legacy::POLY_G4 legacyTransitionEdgePackets[kFrameBufferCount];
// decomp: vs_battle_screenTransitionPolyF4
// (vs_battle_screenTransitionPolyF4@BATTLE/BATTLE.PRG)
extern VS::Legacy::POLY_F4 legacyTransitionFadePackets[kFrameBufferCount];

namespace layout {
inline constexpr std::size_t kLegacyTransitionWipePacketBytes = 0x30;
static_assert(sizeof(LegacyTransitionWipePacket) ==
              kLegacyTransitionWipePacketBytes);
}  // namespace layout
}  // namespace VS::Render
