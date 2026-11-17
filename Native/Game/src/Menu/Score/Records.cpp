// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Menu/Score/Records.h"
#include "Menu/Score/ScoreScreen.h"

// decomp: _clearCount (_clearCount@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_clearCount;

// decomp: _gazetteRows (_gazetteRows@src/MENU/MENU9.PRG/238.c)
Menu_Score_Row Menu_Score_gazetteRows[32];

// decomp: _miscInfo (_miscInfo@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_miscInfo[1646]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _score (_score@src/MENU/MENU9.PRG/238.c)
u_long Menu_Score_score;

// decomp: _statDescriptions (_statDescriptions@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_statDescriptions[460]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _statHeaders (_statHeaders@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_statHeaders[274]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _timeAttackDescriptions (_timeAttackDescriptions@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_timeAttackDescriptions[232]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _timeAttackReferenceTimes (_timeAttackReferenceTimes@src/MENU/MENU9.PRG/238.c)
char const* Menu_Score_kTimeAttackReferenceTimes[] = { "$00:25:00", "$00:30:00", "$00:40:00",
    "$00:50:00", "$01:00:00", "$01:15:00", "$01:00:00", "$01:25:00" };

// decomp: _timeAttacks (_timeAttacks@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_timeAttacks[58]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _titleDescriptions (_titleDescriptions@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_titleDescriptions[784]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _titleText (_titleText@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_titleText[364]; /* contents: disc string table (ModuleData.cpp) */

}  // extern "C"
