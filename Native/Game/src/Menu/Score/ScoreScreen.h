// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_SCORE_SCORESCREEN_H_TYPES
#define VS_MENU_SCORE_SCORESCREEN_H_TYPES
#include "Legacy/Psx.h"

typedef struct Menu_Score_CursorMemory Menu_Score_CursorMemory;
typedef struct Menu_Score_EnemyEntry Menu_Score_EnemyEntry;
typedef struct Menu_Score_Row Menu_Score_Row;
typedef struct Menu_Score_ZudHeader Menu_Score_ZudHeader;

// decomp: _gazetteRow (src/MENU/MENU9.PRG/238.c:363)
typedef struct Menu_Score_Row {
    char selected;
    char opaque1;
    char animationState;
    char disabled;
    short x;
    short rowIndex;
    char* title;
    char* description;
} Menu_Score_Row;

// decomp: _monBinData_t (src/MENU/MENU9.PRG/238.c:377)
typedef struct Menu_Score_EnemyEntry {
    short zudId;
    short classId;
    short killFlagsOffset;
    short killFlagsCount;
    short selected;
    short animationState;
    u_char unlocked;
    u_char prev;
    u_char next;
    u_char reservedF __attribute__((unused));
    char name[28];
} Menu_Score_EnemyEntry;
_Static_assert(sizeof(Menu_Score_EnemyEntry) == 0x2C, "Menu_Score_EnemyEntry must keep its PS1 layout");

// decomp: _zudData_t (src/MENU/MENU9.PRG/592C.c:25)
typedef struct Menu_Score_ZudHeader {
    u_char characterId;
    u_char weapon;
    u_char opaque2;
    u_char weaponMaterial;
    u_char shield;
    u_char shieldMaterial;
    u_char defaultMaterial;
    u_long shpOffset;
    int reservedC;
    u_long weaponWepOffset;
    int reserved14;
    u_long shieldWepOffset;
    int reserved1C;
    u_long commonSeqOffset;
    int commonSeqLen;
    u_long battleSeqOffset;
    int battleSeqLen;
} Menu_Score_ZudHeader;
_Static_assert(sizeof(Menu_Score_ZudHeader) == 0x30, "Menu_Score_ZudHeader must keep its PS1 layout");

// decomp: vs_battle_menu9CursorMemory_t (src/BATTLE/BATTLE.PRG/146C.h:791)
typedef struct Menu_Score_CursorMemory {
    u_char titlePage;
    u_char titleRow;
    u_char encyclopaediaPage;
    u_char encyclopaediaRow;
    u_char recordTimePage;
    u_char recordTimeRow;
    u_char gazettePage;
    u_char gazetteRow;
} Menu_Score_CursorMemory;
_Static_assert(sizeof(Menu_Score_CursorMemory) == 0x8, "Menu_Score_CursorMemory must keep its PS1 layout");

#endif // VS_MENU_SCORE_SCORESCREEN_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_SCORE_SCORESCREEN_H_DECLS)
#define VS_MENU_SCORE_SCORESCREEN_H_DECLS
#include "DiscQueue.h"
#include "Menu/Framework/Framework.h"

extern int Menu_Score_layoutPadding1;;
extern int Menu_Score_layoutPadding2[2];;
extern const CVECTOR Menu_Score_kBgColorStops0[];
extern const CVECTOR Menu_Score_kBgColorStops1[];
extern int Menu_Score_buttonHeld;;
extern SVECTOR Menu_Score_cameraAngles;;
extern VECTOR Menu_Score_cameraAnglesBackup;;
extern int Menu_Score_cameraDistance;;
extern short Menu_Score_cameraHeightOffset;;
extern VECTOR Menu_Score_cameraPositionBackup;;
extern u_short Menu_Score_characterRank;;
extern u_short* Menu_Score_enemyDescriptions;;
extern short Menu_Score_enemySkipDelay;;
extern int Menu_Score_hideUi;;
extern u_short Menu_Score_initCameraDistance;;
extern u_short Menu_Score_mapCompletion;;
extern int Menu_Score_menuState;;
extern u_short Menu_Score_menuText[];
extern Content_DiscQueueSlot* Menu_Score_monBinCdQueueSlot;;
extern Menu_Score_EnemyEntry* Menu_Score_monBinData;;
extern const Content_DiscFile Menu_Score_kMonBinFile;
extern int Menu_Score_nextState;;
extern char Menu_Score_unusedNoString[] __attribute__((unused));
extern u_short Menu_Score_openedChestCount;;
extern u_short Menu_Score_rankText[];
extern int Menu_Score_rotationColor;;
extern short Menu_Score_selectedEnemy;;
extern char Menu_Score_unusedYesString[] __attribute__((unused));
extern int Menu_Score_zoomColor;;
void Menu_Score_RenderScrollableRow(Menu_Framework_MenuRow* menuItem);
void Menu_Score_RestoreTopMenuCursor(int rowCount, int absoluteRow, char* text[]);
int Menu_Score_GetSelectedTopMenuRow(void);
void Menu_Score_ClearMenuExceptRow8(void);
void Menu_Score_PollMenuReady(void);
int Menu_Score_UpdateScreen(u_char* state);
int Menu_Score_CalculateProgress(void);
int Menu_Score_Update(void);
void Menu_Score_RestoreCamera(void);
int Menu_Score_UpdateTopMenu(int initialize);
void Menu_Score_BuildViewerCamera(MATRIX* cameraMatrix);
void Menu_Score_DrawEnemyBanner(int x, int y, int w, int h, int color);
void Menu_Score_DrawGradientPanel(int x, int y, int w, int h, int color);
void Menu_Score_DrawFooterBackground(int x);
void Menu_Score_RenderOverview(int animStep);
void Menu_Score_DrawViewerEdgeFade(void);
void Menu_Score_ScoreScreen_ResetMenu9Statics(void);

#endif // VS_MENU_SCORE_SCORESCREEN_H_DECLS
