// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_SCORE_RECORDS_H_TYPES
#define VS_MENU_SCORE_RECORDS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_SCORE_RECORDS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_SCORE_RECORDS_H_DECLS)
#define VS_MENU_SCORE_RECORDS_H_DECLS
#include "Menu/Score/ScoreScreen.h"

extern u_short Menu_Score_clearCount;;
extern Menu_Score_Row Menu_Score_gazetteRows[32];;
extern u_short Menu_Score_miscInfo[];
extern u_long Menu_Score_score;;
extern u_short Menu_Score_statDescriptions[];
extern u_short Menu_Score_statHeaders[];
extern u_short Menu_Score_timeAttackDescriptions[];
extern char const* Menu_Score_kTimeAttackReferenceTimes[];
extern u_short Menu_Score_timeAttacks[];
extern u_short Menu_Score_titleDescriptions[];
extern u_short Menu_Score_titleText[];
void Menu_Score_CreateTitleRows(void);
void Menu_Score_RenderTitles(int animState);
void Menu_Score_DrawTitleNumber(int x, int y, int titleNo, int dimmed);
void Menu_Score_CreateStatisticRows(void);
void Menu_Score_RenderStatistics(int animState);
void Menu_Score_FormatFastestClearTime(char* buf, int rounds, int totalSeconds);
void Menu_Score_CreateTimeTrialRows(void);
void Menu_Score_LeaveTimeTrialMenu(void);
void Menu_Score_RenderTimeTrialRecords(int selected);
void Menu_Score_DetermineRiskbreakerRank(void);
void Menu_Score_AwardTitles(void);
void Menu_Score_CalculateScore(void);
void Menu_Score_Records_ResetMenu9Statics(void);

#endif // VS_MENU_SCORE_RECORDS_H_DECLS
