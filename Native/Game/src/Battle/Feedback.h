// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_FEEDBACK_H_TYPES
#define VS_BATTLE_FEEDBACK_H_TYPES
#include "Legacy/Psx.h"


// decomp: src/include/gpu.h:95
#define vs_getXY(x, y) (((y) << 16) | ((x) & 0xFFFF))

// decomp: src/include/gpu.h:112
#define vs_getUV(u, v) ((u) | ((v) << 8))

#endif // VS_BATTLE_FEEDBACK_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_FEEDBACK_H_DECLS)
#define VS_BATTLE_FEEDBACK_H_DECLS
#include "Battle/ValueLabels.h"

extern int Battle_valueCaptionRects[17];
extern short Battle_lockedLabelHalfHeights[22];
extern RECT Battle_lockedLabelPieces[4];
extern Battle_FloatingLabel (*Battle_floatingLabels)[32];
extern DVECTOR* Battle_placedLabelPositions;
extern char Battle_actorMarkers[40];
extern short Battle_actorMarkersHidden;
extern short Battle_abilityTimingCueFrames;
extern int Battle_actorMarkerSides;
extern short Battle_statusLabelStepTimer;
extern short Battle_ashleyStatusLabelsSuppressed;
extern short Battle_placedLabelCount;
extern short Battle_floatingLabelCount;
extern SVECTOR Battle_abilityTimingCuePosition;
extern int const Battle_kMarkerFrameCorners[][4];
extern u_short const Battle_kMarkerFrameTexCoords[][4];
extern int const Battle_kMarkerIconOffsets[];
extern RECT const Battle_kLabelCaptionRects[];
extern struct Battle_StatusLabelLayout const Battle_kStatusLabelLayouts[];
extern CVECTOR const Battle_kStatusLabelColors[];
extern RECT const Battle_kStatChangeFirstWordRects[];
extern RECT const Battle_kStatChangeSecondWordRects[];

#endif // VS_BATTLE_FEEDBACK_H_DECLS
