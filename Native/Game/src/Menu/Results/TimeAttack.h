// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_RESULTS_TIMEATTACK_H_TYPES
#define VS_MENU_RESULTS_TIMEATTACK_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_RESULTS_TIMEATTACK_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_RESULTS_TIMEATTACK_H_DECLS)
#define VS_MENU_RESULTS_TIMEATTACK_H_DECLS
#include "DiscQueue.h"

extern Content_DiscFile const Menu_Results_kDisFiles[];
extern int Menu_Results_newTimeSlot;;
extern int Menu_Results_screenState;;
extern int Menu_Results_screenTimer;;
extern int Menu_Results_timeTrialTime;;
int Menu_Results_InitializeTimeAttackResults(void);
int Menu_Results_InitializeTimeAttackStart(void);
int Menu_Results_UpdateTimeAttackResults(void);
int Menu_Results_UpdateTimeAttackStart(void);
void Menu_Results_DrawTimeAttackHeading(int x, int y, int timer);
void Menu_Results_DrawCourseNumber(int x, int y, int timer);
void Menu_Results_DrawBossName(int x, int y, int timer);
void Menu_Results_DrawBestTime(int x, int y, int timer);
void Menu_Results_DrawStartCountdown(int x, int y, int timer);
void Menu_Results_PulseTimeAttackClut(int arg0);
void Menu_Results_DrawTimeAttackResultsHeading(int x, int y, int timer);
void Menu_Results_DrawTimeAttackRating(int x, int y, int arg2);
void Menu_Results_DrawTimeAttackRatingWipe(int x, int y, int arg2);
void Menu_Results_DrawTimeAttackTime(int arg0, int arg1, int arg2);
void Menu_Results_DrawCountingTimeAttackTime(int x, int y, int timer);
void Menu_Results_DrawNewRecord(int x, int y, int timer);
void Menu_Results_DrawLeaderboard(int x, int y, int timer);
void Menu_Results_DrawLeaderboardEntry(int x, int y, int slot, int timer);
void Menu_Results_TimeAttack_ResetMenufStatics(void);

#endif // VS_MENU_RESULTS_TIMEATTACK_H_DECLS
