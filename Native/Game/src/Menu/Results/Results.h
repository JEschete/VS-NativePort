// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_RESULTS_RESULTS_H_TYPES
#define VS_MENU_RESULTS_RESULTS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Menu_Results_BuffReel Menu_Results_BuffReel;

// decomp: BuffReelStats (src/MENU/MENUF.PRG/3B8.c:708)
enum Menu_Results_BuffReelStat {
    Menu_Results_BuffReelStat_Str = 13,
    Menu_Results_BuffReelStat_Agi,
    Menu_Results_BuffReelStat_Int,
    Menu_Results_BuffReelStat_Hp,
    Menu_Results_BuffReelStat_Mp
};
typedef enum Menu_Results_BuffReelStat Menu_Results_BuffReelStat;

// decomp: _disFileIndices (src/MENU/MENUF.PRG/3B8.c:44)
enum Menu_Results_DisFile { Menu_Results_DisFile_Rank, Menu_Results_DisFile_Time, Menu_Results_DisFile_Attack, Menu_Results_DisFile_Iq, Menu_Results_DisFile_Esc };
typedef enum Menu_Results_DisFile Menu_Results_DisFile;

// decomp: disIndices (src/MENU/MENUF.PRG/3B8.c:491)
enum Menu_Results_DisIndex {
    Menu_Results_DisIndex_CommonZero,
    Menu_Results_DisIndex_Rank0Comma = 10,
    Menu_Results_DisIndex_Rank0Bonus,
    Menu_Results_DisIndex_Rank0RiskbreakerRank,
    Menu_Results_DisIndex_Rank0Str,
    Menu_Results_DisIndex_Rank0Agl,
    Menu_Results_DisIndex_Rank0Int,
    Menu_Results_DisIndex_Rank0Hp,
    Menu_Results_DisIndex_Rank0Mp,
    Menu_Results_DisIndex_Rank0Score,
    Menu_Results_DisIndex_Rank0Percent,
    Menu_Results_DisIndex_Rank0Pts,
    Menu_Results_DisIndex_Rank0MapCompleted,
    Menu_Results_DisIndex_Rank0Congratulations,
    Menu_Results_DisIndex_Rank0Plus = 24,
    Menu_Results_DisIndex_Rank0Minus,
    Menu_Results_DisIndex_Rank0Colon,
    Menu_Results_DisIndex_Rank1Grand,
    Menu_Results_DisIndex_Rank1Master,
    Menu_Results_DisIndex_Rank1Breaker,
    Menu_Results_DisIndex_Rank1Paladin,
    Menu_Results_DisIndex_Rank1Gladiator,
    Menu_Results_DisIndex_Rank1Raging,
    Menu_Results_DisIndex_Rank1Berserker,
    Menu_Results_DisIndex_Rank1Radiant,
    Menu_Results_DisIndex_Rank1Couragous,
    Menu_Results_DisIndex_Rank1Adventurer,
    Menu_Results_DisIndex_Rank1Knight,
    Menu_Results_DisIndex_Rank1Archer = 39,
    Menu_Results_DisIndex_Rank1DareDevil = 42,
    Menu_Results_DisIndex_Rank1Blade = 44,
    Menu_Results_DisIndex_Rank2Of = 48,
    Menu_Results_DisIndex_Rank2Mystic = 55,
    Menu_Results_DisIndex_Rank2Wanderer,
    Menu_Results_DisIndex_Rank2Destroyer,
    Menu_Results_DisIndex_Rank2Gladiator,
    Menu_Results_DisIndex_Rank2Spectrebane,
    Menu_Results_DisIndex_Rank2Normal,
    Menu_Results_DisIndex_Rank2Phantom,
    Menu_Results_DisIndex_Rank2Fighter,
    Menu_Results_DisIndex_Rank2Demon,
    Menu_Results_DisIndex_Rank2Eater,
    Menu_Results_DisIndex_Rank2Agent,
    Menu_Results_DisIndex_Rank2Dragon,
    Menu_Results_DisIndex_Rank2Slayer,
    Menu_Results_DisIndex_Rank2Cartographer,
    Menu_Results_DisIndex_Time0NewRecord = 69,
    Menu_Results_DisIndex_Time0TimeAttackResults,
    Menu_Results_DisIndex_Time0Time = 72,
    Menu_Results_DisIndex_Time0Prime,
    Menu_Results_DisIndex_Time0DoublePrime,
    Menu_Results_DisIndex_Time0First,
    Menu_Results_DisIndex_Time0Excellent = 78,
    Menu_Results_DisIndex_Time0Dash = 83,
    Menu_Results_DisIndex_Attack0Course,
    Menu_Results_DisIndex_Attack0Minotaur,
    Menu_Results_DisIndex_Attack0Death = 91,
    Menu_Results_DisIndex_Attack0OgreZombie = 93,
    Menu_Results_DisIndex_Attack0BestTime = 95,
    Menu_Results_DisIndex_Attack0Three,
    Menu_Results_DisIndex_Attack0TimeAttack = 100,
    Menu_Results_DisIndex_Iq0EvolveOrDie = 102,
    Menu_Results_DisIndex_Iq0AverageTime = 104,
    Menu_Results_DisIndex_Iq0Foodchain = 105,
    Menu_Results_DisIndex_Iq1GameDesigner = 107,
    Menu_Results_DisIndex_Esc0GivingUp = 123,
    Menu_Results_DisIndex_Esc0Chicken = 125,
};
typedef enum Menu_Results_DisIndex Menu_Results_DisIndex;

// decomp: _buffReels_t (src/MENU/MENUF.PRG/3B8.c:18)
typedef struct Menu_Results_BuffReel {
    short stat;
    short amount;
} Menu_Results_BuffReel;

#endif // VS_MENU_RESULTS_RESULTS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_RESULTS_RESULTS_H_DECLS)
#define VS_MENU_RESULTS_RESULTS_H_DECLS
#include "Render/Images.h"

extern int Menu_Results_buffReelIndex;;
extern u_int Menu_Results_buffReelSelection;;
extern Menu_Results_BuffReel Menu_Results_buffReels[][16];
extern int Menu_Results_clearCount;;
extern Render_TextureRegion Menu_Results_disMap[];
extern u_int Menu_Results_elementAnimationState;;
extern int Menu_Results_incrementingMapCompletion;;
extern int Menu_Results_incrementingScore;;
extern u_int Menu_Results_mapCompletion;;
extern int Menu_Results_onScreenMapCompletion;;
extern int Menu_Results_onScreenScore;;
extern int Menu_Results_rank;;
extern u_char Menu_Results_riskbreakerRanks[][4];
extern u_int Menu_Results_score;;
int Menu_Results_UpdateScreen(u_char* state);
int Menu_Results_InitializeScreen(void);
int Menu_Results_InitializeGameClear(void);
void Menu_Results_QueueResultsMusic(void);
int Menu_Results_UpdatePresentation(void);
int Menu_Results_UpdateGameClear(void);
void Menu_Results_DrawCongratulations(int x, int y, int timer);
void Menu_Results_DrawScore(int x, int y, int timer, int arg3 __attribute__((unused)));
void Menu_Results_DrawCountingScore(int x, int y, int timer, int arg3 __attribute__((unused)));
void Menu_Results_DrawMapCompletion(int x, int y, int timer, int arg3 __attribute__((unused)));
void Menu_Results_DrawCountingMapCompletion(
    int arg0, int arg1, int arg2, int arg3 __attribute__((unused)));
void Menu_Results_DrawRiskbreakerRankHeading(int x, int y, int timer);
void Menu_Results_DrawRiskbreakerRank(int x, int y, int timer);
void Menu_Results_DrawFadingTexture(int x, int y, int texIndex, int brightness);
void Menu_Results_DrawFadingTextureClut(int x, int y, int texIndex, int brightness, int clut);
void Menu_Results_DrawTime(int x, int y, int timer, int time, int isNewRecord);
void Menu_Results_DrawBonusReel(int x, int y, int timer, int arg3);
void Menu_Results_DrawPressButtonPrompt(int x, int y, int arg2, int arg3);
void Menu_Results_DrawBonusReelTexture(int x, int y, int texId);
void Menu_Results_DetermineRiskbreakerRank(void);
void Menu_Results_CalculateScore(void);
void Menu_Results_ApplyHealingScoreBonus(void);
int Menu_Results_AshleyTotalStrength(void);
int Menu_Results_AshleyTotalIntelligence(void);
int Menu_Results_AshleyTotalAgility(void);
void Menu_Results_ApplyStrengthBonus(int amount);
void Menu_Results_ApplyIntelligenceBonus(int amount);
void Menu_Results_ApplyAgilityBonus(int amount);
void Menu_Results_ApplyHpBonus(int amount);
void Menu_Results_ApplyMpBonus(int amount);
void Menu_Results_ResetMenufStatics(void);

#endif // VS_MENU_RESULTS_RESULTS_H_DECLS
