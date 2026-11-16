// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_SCORE_RECORDS_H_TYPES
#define VS_MENU_SCORE_RECORDS_H_TYPES
#include "Legacy/Psx.h"


// decomp: src/include/gpu.h:96
#define vs_getXY_2(x, y) (((x) & 0xFFFF) | ((y) << 16))

// decomp: src/include/gpu.h:102
#define vs_getRGB(r, g, b) vs_getRGB0(0, (r), (g), (b))

// decomp: src/include/gpu.h:103
#define vs_getRGB0(code, r, g, b)                                                        \
    (((code) << 24) | (((b) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((r) & 0xFF))

#endif // VS_MENU_SCORE_RECORDS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_SCORE_RECORDS_H_DECLS)
#define VS_MENU_SCORE_RECORDS_H_DECLS
#include "Menu/Score/ScoreScreen.h"

extern u_short Menu_Score_clearCount;;
extern Menu_Score_Row Menu_Score_gazetteRows[32];;
extern u_short Menu_Score_miscInfo[1646]; /* contents: disc string table (ModuleData.cpp) */;
extern u_long Menu_Score_score;;
extern u_short Menu_Score_statDescriptions[460]; /* contents: disc string table (ModuleData.cpp) */;
extern u_short Menu_Score_statHeaders[274]; /* contents: disc string table (ModuleData.cpp) */;
extern u_short Menu_Score_timeAttackDescriptions[232]; /* contents: disc string table (ModuleData.cpp) */;
extern char const* Menu_Score_kTimeAttackReferenceTimes[];
extern u_short Menu_Score_timeAttacks[58]; /* contents: disc string table (ModuleData.cpp) */;
extern u_short Menu_Score_titleDescriptions[784]; /* contents: disc string table (ModuleData.cpp) */;
extern u_short Menu_Score_titleText[364]; /* contents: disc string table (ModuleData.cpp) */;
void Menu_Score_CreateTitleRows(void);
void Menu_Score_RenderTitles(int animState);
void Menu_Score_DrawTitleNumber(int x, int y, int titleNo, int dimmed);
void Menu_Score_CreateStatisticRows(void);
void Menu_Score_RenderStatistics(int animState);
void Menu_Score_FormatFastestClearTime(char* buf, int rounds, int totalSeconds);
void Menu_Score_CreateTimeTrialRows(void);
void Menu_Score_LeaveTimeTrialMenu(void);
void Menu_Score_RenderTimeTrialRecords(int selected);
void Menu_Score_Records_ResetMenu9Statics(void);

#endif // VS_MENU_SCORE_RECORDS_H_DECLS
