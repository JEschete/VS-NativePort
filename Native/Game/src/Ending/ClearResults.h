// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ENDING_CLEARRESULTS_H_TYPES
#define VS_ENDING_CLEARRESULTS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ENDING_CLEARRESULTS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ENDING_CLEARRESULTS_H_DECLS)
#define VS_ENDING_CLEARRESULTS_H_DECLS

extern u_char Ending_kRankHeadingWidth;
extern CVECTOR Ending_congratulationsTint[2];
extern CVECTOR Ending_scoreTint[48 / sizeof(CVECTOR)];
extern CVECTOR Ending_countingScoreTint[40 / sizeof(CVECTOR)];
extern CVECTOR Ending_mapCompletionTint[32 / sizeof(CVECTOR)];
extern CVECTOR Ending_countingMapCompletionTint[24 / sizeof(CVECTOR)];
extern CVECTOR Ending_rankHeadingTint[4];
extern CVECTOR Ending_rankLabelTint[8 / sizeof(CVECTOR)];
extern u_int Ending_riskbreakerRankIndex;
extern int Ending_clearCount;
extern u_int Ending_finalScore;
extern int Ending_mapCompletion;
extern u_int Ending_displayedMapCompletion;
extern int Ending_displayedScore;
extern int Ending_resultsLoadPhase;
extern int Ending_resultsAnimationFrame;
extern int Ending_isLeavingResults;
extern int Ending_isScoreIncrementing;
extern int Ending_isMapCompletionIncrementing;
void Ending_CommitClearProgress(void);
void Ending_UpdateClearProgress(void);
void Ending_AwardClearTitles(void);
void Ending_ShowClearResults(void);
int Ending_LoadClearResultsStep(void);
int Ending_DrawClearResults(void);
void Ending_DrawRankBackground(int fade_frame);
void Ending_DrawCongratulations(int x, int y, int fade_frame);
void Ending_DrawScore(int x, int y, int fade_frame, int arg3 __attribute__((unused)));
void Ending_DrawCountingScore(
    int x, int y, int animation_frame, int arg3 __attribute__((unused)));
void Ending_DrawMapCompletion(
    int x, int y, int fade_frame, int arg3 __attribute__((unused)));
void Ending_DrawCountingMapCompletion(
    int x, int y, int fade_frame, int arg3 __attribute__((unused)));
void Ending_DrawRiskbreakerRankHeading(int x, int y, int timer);
void Ending_DrawRiskbreakerRank(int x, int y, int timer);
void Ending_DetermineRiskbreakerRank(void);
void Ending_CalculateClearScore(void);

#endif // VS_ENDING_CLEARRESULTS_H_DECLS
