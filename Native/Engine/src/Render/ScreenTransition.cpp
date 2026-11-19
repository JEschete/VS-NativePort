// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Render/ScreenTransition.hpp"

// decomp: vs_battle_screenTransitionDrMode (vs_battle_screenTransitionDrMode@BATTLE/BATTLE.PRG)
DR_MODE VS::Render::legacyTransitionDrawModes[VS::Render::kFrameBufferCount];

// decomp: vs_battle_screenTransitionWipePolyF4 (vs_battle_screenTransitionWipePolyF4@BATTLE/BATTLE.PRG)
VS::Render::LegacyTransitionWipePacket VS::Render::legacyTransitionWipePackets[VS::Render::kFrameBufferCount];

// decomp: vs_battle_screenTransitionEdgePolyG4 (vs_battle_screenTransitionEdgePolyG4@BATTLE/BATTLE.PRG)
POLY_G4 VS::Render::legacyTransitionEdgePackets[VS::Render::kFrameBufferCount];

// decomp: vs_battle_screenTransitionPolyF4 (vs_battle_screenTransitionPolyF4@BATTLE/BATTLE.PRG)
POLY_F4 VS::Render::legacyTransitionFadePackets[VS::Render::kFrameBufferCount];

