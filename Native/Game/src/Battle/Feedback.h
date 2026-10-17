// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_FEEDBACK_H_TYPES
#define VS_BATTLE_FEEDBACK_H_TYPES
#include "Legacy/Psx.h"


// decomp: src/include/gpu.h:108
#define vs_getRGB888(r, g, b) (((r) & 0xFF) | (((g) & 0xFF) << 8) | (((b) & 0xFF) << 16))

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
void Battle_ResetFloatingLabels(void);
int Battle_QueueWorldLockedLabel(u_short* arg0);
int Battle_QueueActorValueLabel(int arg0, int arg1, u_int arg2, int arg3);
int Battle_QueueAbilityTimingResult(int arg0);
int Battle_QueueChainCount(int arg0);
int Battle_QueueActorStatusLabels(int arg0, int arg1, int arg2, int arg3);
int Battle_QueueActorStatusLabel(int arg0, int arg1, int arg2, int arg3);
void Battle_UpdateAndDrawFloatingLabels(void);
void Battle_PlaceActorValueLabel(short* pos, Battle_FloatingLabel* label);
void Battle_AdvanceFloatingLabelQueue(Battle_FloatingLabel* arg0);
void Battle_DrawActorValueLabel(int x, int y, Battle_FloatingLabel* p);
int Battle_DrawValueLabelCaption(int x, int y, Battle_FloatingLabel* p);
void Battle_DrawLockedFloatingLabel(int arg0, int arg1, Battle_FloatingLabel* arg2);
void Battle_DrawActorStatusLabel(int x, int y, Battle_FloatingLabel* p);
void Battle_DrawStatusRemovalMarker(int arg0, int arg1, int arg2);
void Battle_DrawAbilityTimingResult(int x, int y, Battle_FloatingLabel* p);
void Battle_DrawChainCount(int x, int y, Battle_FloatingLabel* p);
void Battle_SetActorMarkersHidden(int arg0);
void Battle_SetActorMarker(int arg0, int arg1);
void Battle_DrawActorMarker(int actor, int kind);
void Battle_DrawActorMarkers(void);
void* Battle_ReserveFloatingLabel(void);
void Battle_CompactFloatingLabels(int arg0);
void Battle_RemoveFloatingLabel(int arg0);
void Battle_DrawOffscreenActorArrow(SVECTOR* position, int direction);
int Battle_QueueEquipmentStatChange(int arg0, int arg1, int arg2);
void Battle_DrawEquipmentStatChange(int x, int y, Battle_FloatingLabel* p);
void Battle_ShowAbilityTimingCue(int arg0);
void Battle_DrawAbilityTimingCue(void);
void Battle_CommitEquipmentStatChanges(void);
void Battle_SuppressAshleyStatusLabels(void);

#endif // VS_BATTLE_FEEDBACK_H_DECLS
