// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_STATE_PROGRESS_H_TYPES
#define VS_STATE_PROGRESS_H_TYPES
#include "Legacy/Psx.h"

typedef struct State_ArtsStatus State_ArtsStatus;
typedef struct State_RoomVisitFlags State_RoomVisitFlags;
typedef struct State_ScoreData State_ScoreData;
typedef struct State_TrialScore State_TrialScore;

// decomp: vs_main_artsStatus_t (src/SLUS_010.40/main.h:375)
typedef struct State_ArtsStatus {
    u_char artsLearned[12];
    struct {
        u_short weaponCategoryKills[10];
        u_short battleAbilitiesUnlocked;
        u_short totalKills;
    } kills;
} State_ArtsStatus;

// decomp: vs_main_mapStatus_t (src/SLUS_010.40/main.h:394)
typedef struct State_RoomVisitFlags {
    int roomFlags[16];
    int areaFlags[2];
} State_RoomVisitFlags;
_Static_assert(sizeof(State_RoomVisitFlags) == 0x48, "State_RoomVisitFlags must keep its PS1 layout");

// decomp: vs_trialScore_t (src/SLUS_010.40/main.h:179)
typedef struct State_TrialScore {
    u_int time : 24;
    u_int round : 7;
    u_int difficulty : 1;
} State_TrialScore;
_Static_assert(sizeof(State_TrialScore) == 0x4, "State_TrialScore must keep its PS1 layout");

// decomp: vs_main_scoredata_t (src/SLUS_010.40/main.h:202)
typedef struct State_ScoreData {
    int titles;
    u_short enemyKills[6];
    int reserved10;
    u_short weaponAttacks[10];
    State_TrialScore bossTimeTrialScores[8][3];
    u_short maxChain;
    u_short reserved8A;
    u_int clearCount;
    u_int completionTimeMinutes;
    u_int mapCompletion;
    u_int openedChestCount;
    int enemyKillFlags[26];
    u_int streakScore;
    u_int miscScore;
    char killStreakEnemyClass;
    char killStreakEnemyCount;
    char weaponKillStreakBladeCategory;
    char weaponKillStreak;
    u_short chainStreak;
    u_short healCount;
} State_ScoreData;
_Static_assert(sizeof(State_ScoreData) == 0x114, "State_ScoreData must keep its PS1 layout");

#endif // VS_STATE_PROGRESS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_STATE_PROGRESS_H_DECLS)
#define VS_STATE_PROGRESS_H_DECLS

extern char State_learnedActionFlags[32];
extern State_RoomVisitFlags State_roomVisitProgress;

#endif // VS_STATE_PROGRESS_H_DECLS
