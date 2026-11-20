// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Render/ScreenTransition.hpp"

#include "Legacy/Psx.hpp"

using VS::Legacy::DR_MODE;
using VS::Legacy::POLY_F4;
using VS::Legacy::POLY_G4;

// decomp: vs_battle_screenTransitionDrMode
// (vs_battle_screenTransitionDrMode@BATTLE/BATTLE.PRG)
DR_MODE VS::Render::legacyTransitionDrawModes[VS::Render::kFrameBufferCount];

// decomp: vs_battle_screenTransitionWipePolyF4
// (vs_battle_screenTransitionWipePolyF4@BATTLE/BATTLE.PRG)
VS::Render::LegacyTransitionWipePacket
    VS::Render::legacyTransitionWipePackets[VS::Render::kFrameBufferCount];

// decomp: vs_battle_screenTransitionEdgePolyG4
// (vs_battle_screenTransitionEdgePolyG4@BATTLE/BATTLE.PRG)
POLY_G4 VS::Render::legacyTransitionEdgePackets[VS::Render::kFrameBufferCount];

// decomp: vs_battle_screenTransitionPolyF4
// (vs_battle_screenTransitionPolyF4@BATTLE/BATTLE.PRG)
POLY_F4 VS::Render::legacyTransitionFadePackets[VS::Render::kFrameBufferCount];
