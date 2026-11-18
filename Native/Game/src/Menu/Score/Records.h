// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


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
