// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Feedback.h"
#include "Battle/ValueLabels.h"
#include "Menu/Status/ActorStatus.h"

// decomp: D_800E861C (D_800E861C@BATTLE/BATTLE.PRG)
int Battle_valueCaptionRects[17]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8634 (D_800E8634@BATTLE/BATTLE.PRG)
short Battle_lockedLabelHalfHeights[22]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8640 (D_800E8640@BATTLE/BATTLE.PRG)
RECT Battle_lockedLabelPieces[4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F1BAC (D_800F1BAC@BATTLE/BATTLE.PRG)
Battle_FloatingLabel (*Battle_floatingLabels)[32];

// decomp: D_800F1CBC (D_800F1CBC@BATTLE/BATTLE.PRG)
DVECTOR* Battle_placedLabelPositions;

// decomp: D_800F2260 (D_800F2260@BATTLE/BATTLE.PRG)
char Battle_actorMarkers[40];

// decomp: D_800F2270 (D_800F2270@BATTLE/BATTLE.PRG)
short Battle_actorMarkersHidden;

// decomp: D_800F2272 (D_800F2272@BATTLE/BATTLE.PRG)
short Battle_abilityTimingCueFrames;

// decomp: D_800F2274 (D_800F2274@BATTLE/BATTLE.PRG)
int Battle_actorMarkerSides;

// decomp: D_800F2278 (D_800F2278@BATTLE/BATTLE.PRG)
short Battle_statusLabelStepTimer;

// decomp: D_800F227A (D_800F227A@BATTLE/BATTLE.PRG)
short Battle_ashleyStatusLabelsSuppressed;

// decomp: D_800F227C (D_800F227C@BATTLE/BATTLE.PRG)
short Battle_placedLabelCount;

// decomp: D_800F227E (D_800F227E@BATTLE/BATTLE.PRG)
short Battle_floatingLabelCount;

// decomp: D_800F2280 (D_800F2280@BATTLE/BATTLE.PRG)
SVECTOR Battle_abilityTimingCuePosition;

// decomp: D_80068F04 (D_80068F04@src/BATTLE/BATTLE.PRG/2842C.c)
int const Battle_kMarkerFrameCorners[][4] = { { vs_getXY(-6, -22), vs_getXY(18, -22),
                                         vs_getXY(-6, 1), vs_getXY(18, 1) },
    { vs_getXY(-17, -22), vs_getXY(7, -22), vs_getXY(-17, 1), vs_getXY(7, 1) },
    { vs_getXY(-10, -29), vs_getXY(20, -29), vs_getXY(-10, 1), vs_getXY(20, 1) },
    { vs_getXY(-19, -29), vs_getXY(9, -29), vs_getXY(-19, 1), vs_getXY(9, 1) },
    { vs_getXY(-9, -16), vs_getXY(11, -16), vs_getXY(-9, -3), vs_getXY(11, -3) },
    { vs_getXY(-10, -16), vs_getXY(10, -16), vs_getXY(-10, -3), vs_getXY(10, -3) } };

// decomp: D_80068F64 (D_80068F64@src/BATTLE/BATTLE.PRG/2842C.c)
u_short const Battle_kMarkerFrameTexCoords[][4] = { { vs_getUV(64, 16), vs_getUV(88, 16),
                                             vs_getUV(64, 39), vs_getUV(88, 39) },
    { vs_getUV(87, 16), vs_getUV(63, 16), vs_getUV(87, 39), vs_getUV(63, 39) },
    { vs_getUV(121, 17), vs_getUV(151, 17), vs_getUV(121, 47), vs_getUV(151, 47) },
    { vs_getUV(150, 17), vs_getUV(120, 17), vs_getUV(150, 47), vs_getUV(120, 47) },
    { vs_getUV(160, 17), vs_getUV(180, 17), vs_getUV(160, 30), vs_getUV(180, 30) },
    { vs_getUV(179, 17), vs_getUV(159, 17), vs_getUV(179, 30), vs_getUV(159, 30) } };

// decomp: D_80068F94 (D_80068F94@src/BATTLE/BATTLE.PRG/2842C.c)
int const Battle_kMarkerIconOffsets[] = { vs_getXY(-2, -16), vs_getXY(-14, -16), vs_getXY(0, -21),
    vs_getXY(-7, -21), vs_getXY(3, -11), vs_getXY(3, -11) };

// decomp: D_80068FAC (D_80068FAC@src/BATTLE/BATTLE.PRG/2842C.c)
RECT const Battle_kLabelCaptionRects[] = { { 0, 64, 36, 12 }, { 0, 64, 24, 12 },
    { 24, 64, 20, 12 }, { 44, 64, 24, 12 }, { 224, 64, 16, 12 }, { 212, 76, 32, 12 },
    { 150, 64, 26, 12 }, { 68, 64, 40, 12 }, { 108, 64, 30, 12 }, { 138, 64, 50, 12 },
    { 188, 64, 36, 12 }, { 0, 76, 52, 12 }, { 52, 76, 30, 12 }, { 144, 88, 58, 12 },
    { 120, 76, 60, 12 }, { 180, 76, 30, 12 }, { 136, 88, 8, 12 }, { 104, 88, 32, 12 },
    { 76, 88, 40, 12 }, { 0, 88, 16, 12 }, { 20, 88, 20, 12 }, { 40, 88, 30, 12 },
    { 72, 88, 32, 12 }, { 102, 76, 16, 12 }, { 220, 100, 18, 12 }, { 152, 52, 26, 12 },
    { 182, 40, 30, 12 }, { 214, 40, 24, 12 }, { 152, 52, 28, 12 }, { 180, 52, 40, 12 },
    { 220, 52, 30, 12 }, { 108, 64, 30, 12 }, { 216, 28, 28, 12 } };

// decomp: D_800690B4 (D_800690B4@BATTLE/BATTLE.PRG)
struct Battle_StatusLabelLayout const Battle_kStatusLabelLayouts[] = { { 1, { 31, 0 }, 0 }, { 2, { 29, 27 }, 0 }, { 2, { 30, 27 }, 0 },
    { 2, { 28, 26 }, 0 }, { 2, { 32, 27 }, 0 }, { 2, { 1, 5 }, 0 }, { 2, { 1, 4 }, 1 },
    { 2, { 2, 5 }, 0 }, { 2, { 2, 4 }, 1 }, { 2, { 3, 5 }, 0 }, { 2, { 3, 4 }, 1 },
    { 1, { 7, 0 }, 2 }, { 1, { 8, 0 }, 2 }, { 1, { 9, 0 }, 2 }, { 1, { 10, 0 }, 2 },
    { 1, { 11, 0 }, 2 }, { 1, { 12, 0 }, 2 }, { 1, { 13, 0 }, 3 }, { 1, { 14, 0 }, 3 },
    { 2, { 15, 5 }, 0 }, { 2, { 15, 4 }, 1 }, { 2, { 16, 19 }, 2 }, { 2, { 16, 20 }, 2 },
    { 2, { 16, 21 }, 2 }, { 2, { 16, 22 }, 2 }, { 2, { 17, 19 }, 2 },
    { 2, { 17, 20 }, 2 }, { 2, { 17, 21 }, 2 }, { 2, { 17, 22 }, 2 }, { 0, { 0, 0 }, 0 },
    { 0, { 0, 0 }, 0 }, { 0, { 0, 0 }, 0 } };

// decomp: D_80069134 (D_80069134@src/BATTLE/BATTLE.PRG/2842C.c)
CVECTOR const Battle_kStatusLabelColors[] = { { 208, 208, 208 }, { 64, 192, 255 },
    { 255, 208, 48 }, { 160, 224, 96 } };

// decomp: D_80069144 (D_80069144@src/BATTLE/BATTLE.PRG/2842C.c)
RECT const Battle_kStatChangeFirstWordRects[] = { { 72, 160, 32, 8 }, { 76, 168, 28, 8 },
    { 78, 176, 26, 8 }, { 78, 184, 26, 8 }, { 82, 192, 22, 8 }, { 82, 200, 22, 8 },
    { 82, 208, 22, 8 } };

// decomp: D_8006917C (D_8006917C@src/BATTLE/BATTLE.PRG/2842C.c)
RECT const Battle_kStatChangeSecondWordRects[] = { { 104, 160, 28, 8 }, { 104, 168, 26, 8 },
    { 104, 176, 30, 8 }, { 104, 184, 36, 8 }, { 104, 192, 32, 8 }, { 104, 200, 18, 8 },
    { 144, 160, 30, 8 }, { 144, 168, 14, 8 }, { 144, 176, 18, 8 }, { 144, 184, 26, 8 },
    { 144, 192, 26, 8 }, { 144, 200, 22, 8 }, { 144, 208, 36, 8 } };

}  // extern "C"
