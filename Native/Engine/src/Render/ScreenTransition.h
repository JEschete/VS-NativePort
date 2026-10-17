// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_SCREENTRANSITION_H_TYPES
#define VS_RENDER_SCREENTRANSITION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Render_LegacyTransitionWipePacket Render_LegacyTransitionWipePacket;

// decomp: vs_battle_screenTransitionWipePoly (src/BATTLE/BATTLE.PRG/146C.h:802)
typedef struct Render_LegacyTransitionWipePacket {
    POLY_F4 coveredQuad;
    POLY_F4 edgeQuad;
} Render_LegacyTransitionWipePacket;
_Static_assert(sizeof(Render_LegacyTransitionWipePacket) == 0x30, "Render_LegacyTransitionWipePacket must keep its PS1 layout");

#endif // VS_RENDER_SCREENTRANSITION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_SCREENTRANSITION_H_DECLS)
#define VS_RENDER_SCREENTRANSITION_H_DECLS

extern DR_MODE Render_legacyTransitionDrawModes[2];
extern Render_LegacyTransitionWipePacket Render_legacyTransitionWipePackets[2];
extern POLY_G4 Render_legacyTransitionEdgePackets[2];
extern POLY_F4 Render_legacyTransitionFadePackets[3];

#endif // VS_RENDER_SCREENTRANSITION_H_DECLS
