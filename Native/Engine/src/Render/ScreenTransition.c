// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Render/ScreenTransition.h"

// decomp: vs_battle_screenTransitionDrMode (vs_battle_screenTransitionDrMode@BATTLE/BATTLE.PRG)
DR_MODE Render_legacyTransitionDrawModes[2];

// decomp: vs_battle_screenTransitionWipePolyF4 (vs_battle_screenTransitionWipePolyF4@BATTLE/BATTLE.PRG)
Render_LegacyTransitionWipePacket Render_legacyTransitionWipePackets[2];

// decomp: vs_battle_screenTransitionEdgePolyG4 (vs_battle_screenTransitionEdgePolyG4@BATTLE/BATTLE.PRG)
POLY_G4 Render_legacyTransitionEdgePackets[2];

// decomp: vs_battle_screenTransitionPolyF4 (vs_battle_screenTransitionPolyF4@BATTLE/BATTLE.PRG)
POLY_F4 Render_legacyTransitionFadePackets[3];

