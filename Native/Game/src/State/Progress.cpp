// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "State/Progress.h"

// decomp: vs_main_actionsLearned (vs_main_actionsLearned@SLUS_010.40)
char State_learnedActionFlags[32];

// decomp: vs_main_mapStatus (vs_main_mapStatus@SLUS_010.40)
State_RoomVisitFlags State_roomVisitProgress;

// decomp: vs_main_scoredata (vs_main_scoredata@SLUS_010.40)
State_ScoreData VS::State::scoreData;

// decomp: vs_main_artsStatus (vs_main_artsStatus@SLUS_010.40)
State_ArtsStatus VS::State::artsStatus;
