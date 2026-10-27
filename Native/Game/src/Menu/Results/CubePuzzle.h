// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_RESULTS_CUBEPUZZLE_H_TYPES
#define VS_MENU_RESULTS_CUBEPUZZLE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_RESULTS_CUBEPUZZLE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_RESULTS_CUBEPUZZLE_H_DECLS)
#define VS_MENU_RESULTS_CUBEPUZZLE_H_DECLS

extern u_short Menu_Results_clutBuf[160];;
extern short Menu_Results_puzzleParTime;;
extern short Menu_Results_puzzleRankCap;;
void Menu_Results_PulseCubePuzzleClut(int blend_weight);
int Menu_Results_InitializeCubePuzzleStart(void);
int Menu_Results_InitializeCubePuzzleResults(void);
int Menu_Results_InitializeCubePuzzleQuit(void);
int Menu_Results_UpdateCubePuzzleStart(void);
int Menu_Results_UpdateCubePuzzleResults(void);
int Menu_Results_UpdateCubePuzzleQuit(void);
void Menu_Results_DrawGivingUp(int draw_y, int fade_timer);
void Menu_Results_DrawChicken(int draw_y, int fade_timer);
void Menu_Results_DrawEvolveOrDie(int x, int y, int step);
void Menu_Results_DrawFoodChain(int x, int y, int timer);
void Menu_Results_DrawAverageTimeHeading(int x, int y, int timer);
void Menu_Results_DrawCubePuzzleParTime(int x, int y, int timer);
void Menu_Results_DrawCubePuzzleRank(int x, int y, int timer, int rank);
void Menu_Results_DrawCountingCubePuzzleTime(int x, int y, int timer);
void Menu_Results_CubePuzzle_ResetMenufStatics(void);

#endif // VS_MENU_RESULTS_CUBEPUZZLE_H_DECLS
