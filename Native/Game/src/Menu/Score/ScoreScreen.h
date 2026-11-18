// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Menu_Score_CursorMemory;
struct Menu_Score_EnemyEntry;
struct Menu_Score_Row;
struct Menu_Score_ZudHeader;

// decomp: _gazetteRow (src/MENU/MENU9.PRG/238.c:363)
struct Menu_Score_Row {
    char selected;
    char opaque1;
    char animationState;
    char disabled;
    short x;
    short rowIndex;
    char* title;
    char* description;
};

// decomp: _monBinData_t (src/MENU/MENU9.PRG/238.c:377)
struct Menu_Score_EnemyEntry {
    short zudId;
    short classId;
    short killFlagsOffset;
    short killFlagsCount;
    short selected;
    short animationState;
    u_char unlocked;
    u_char prev;
    u_char next;
    u_char reservedF;
    char name[28];
};
static_assert(sizeof(Menu_Score_EnemyEntry) == 0x2C, "Menu_Score_EnemyEntry must keep its PS1 layout");

// decomp: _zudData_t (src/MENU/MENU9.PRG/592C.c:25)
struct Menu_Score_ZudHeader {
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
};
static_assert(sizeof(Menu_Score_ZudHeader) == 0x30, "Menu_Score_ZudHeader must keep its PS1 layout");

// decomp: vs_battle_menu9CursorMemory_t (src/BATTLE/BATTLE.PRG/146C.h:791)
struct Menu_Score_CursorMemory {
    u_char titlePage;
    u_char titleRow;
    u_char encyclopaediaPage;
    u_char encyclopaediaRow;
    u_char recordTimePage;
    u_char recordTimeRow;
    u_char gazettePage;
    u_char gazetteRow;
};
static_assert(sizeof(Menu_Score_CursorMemory) == 0x8, "Menu_Score_CursorMemory must keep its PS1 layout");

#include "DiscQueue.h"

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
extern u_short Menu_Score_menuText[34]; /* contents: disc string table (ModuleData.cpp) */;
extern Content_DiscQueueSlot* Menu_Score_monBinCdQueueSlot;;
extern Menu_Score_EnemyEntry* Menu_Score_monBinData;;
extern const Content_DiscFile Menu_Score_kMonBinFile;
extern int Menu_Score_nextState;;
extern char Menu_Score_unusedNoString[];
extern u_short Menu_Score_openedChestCount;;
extern u_short Menu_Score_rankText[192]; /* contents: disc string table (ModuleData.cpp) */;
extern int Menu_Score_rotationColor;;
extern short Menu_Score_selectedEnemy;;
extern char Menu_Score_unusedYesString[];
extern int Menu_Score_zoomColor;;
