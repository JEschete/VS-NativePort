// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_HUD_H_TYPES
#define VS_BATTLE_HUD_H_TYPES
#include "Legacy/Psx.h"

typedef union Battle_HudHighlightWord Battle_HudHighlightWord;

// decomp: (unnamed at D:\Dev\NativePort\VS-NativePort\resources\rood-reverseWin\src\BATTLE\BATTLE.PRG\5BF94.c:451:8) (src/BATTLE/BATTLE.PRG/5BF94.c:451)
union Battle_HudHighlightWord {
    int word;
    u_char bytes[4];
};
typedef union Battle_HudHighlightWord Battle_HudHighlightWord;
_Static_assert(sizeof(Battle_HudHighlightWord) == 0x4, "Battle_HudHighlightWord must keep its PS1 layout");

#endif // VS_BATTLE_HUD_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_HUD_H_DECLS)
#define VS_BATTLE_HUD_H_DECLS
#include "Battle/Actor.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"

extern char* Battle_limbNames[146];
extern char Battle_hudStatusIconsHidden;
extern char Battle_hudHidden;
extern int Battle_hudSlideOffset;
extern char Battle_hudBattleMode;
extern char Battle_battleModeIndicatorStep;
extern union Battle_HudHighlightWord Battle_statusIconCarousel;
extern int Battle_hudGaugePositions[4];
extern int Battle_hudGaugeLabelPositions[4];
extern char Battle_hudGaugeLabelWidths[4];
extern char Battle_hudGaugeLabelTexU[20];
extern u_short Battle_statusIconTexOffsets[32];
extern u_int Battle_limbStatusColors[10];
extern u_short Battle_upperStatusIconFlags[10];
extern char Battle_shortcutHudRequested;
extern int Battle_firstShortcutDigitOffsets[58];
extern u_char Battle_secondShortcutMarkerPositions[216];
extern u_char Battle_secondShortcutIconPositions[208];
extern int Battle_secondShortcutIconSizes[50];
extern u_short Battle_secondShortcutIconTexCoords[92];
extern u_char Battle_firstShortcutMarkerPositions[176];
extern u_char Battle_firstShortcutIconPositions[168];
extern int Battle_firstShortcutIconSizes[40];
extern u_short Battle_firstShortcutIconTexCoords[72];
extern u_char Battle_shortcutHudAnimationStep;
extern int Battle_limbDisplaySpriteOffsets[33];
extern int Battle_limbDisplaySpriteSizes[23];
extern u_int Battle_limbDisplaySpriteTexCoords[13];
extern u_short Battle_battleModeIndicatorWobble;
extern char* Battle_puzzleTimerTextPointer;
extern char Battle_lowerScreenUiState;
extern char const Battle_puzzleTimerText[];
extern int Battle_puzzleTimer;
int Battle_SetGameplayHudHidden(int arg0);
void Battle_DrawHudStatGauge(int colorIndex, int current, int max);
void Battle_DrawSlidingStatusIcon(int arg0, int arg1, int arg2);
int Battle_StatusIconFlags(Battle_ActorStats* arg0);
int Battle_LimbCondition(Battle_LimbStats* limb);
int Battle_ShortcutIconBrightness(int arg0);
void Battle_ShowShortcutHud(void);
void Battle_DrawShortcutButtonMarker(int arg0, int arg1);
void Battle_DrawShortcutHud(void);
void Battle_DrawBattleHud(void);
void Battle_ShowPuzzleTimer(void);
void Battle_HidePuzzleTimer(void);
void Battle_DrawPuzzleTimer(int* value);
void Battle_SetHudHidden(int arg0);
void Battle_SetHudBattleMode(int arg0);

#endif // VS_BATTLE_HUD_H_DECLS
