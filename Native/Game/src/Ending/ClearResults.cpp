// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Ending/ClearResults.h"

// decomp: D_800DB876 (D_800DB876@ENDING/ENDING.PRG)
u_char Ending_kRankHeadingWidth; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DBA7C (D_800DBA7C@ENDING/ENDING.PRG)
CVECTOR Ending_congratulationsTint[3]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DBA88 (D_800DBA88@ENDING/ENDING.PRG)
CVECTOR Ending_scoreTint[48 / sizeof(CVECTOR)]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DBA90 (D_800DBA90@ENDING/ENDING.PRG)
CVECTOR Ending_countingScoreTint[40 / sizeof(CVECTOR)]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DBA98 (D_800DBA98@ENDING/ENDING.PRG)
CVECTOR Ending_mapCompletionTint[32 / sizeof(CVECTOR)]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DBAA0 (D_800DBAA0@ENDING/ENDING.PRG)
CVECTOR Ending_countingMapCompletionTint[24 / sizeof(CVECTOR)]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DBAA8 (D_800DBAA8@ENDING/ENDING.PRG)
CVECTOR Ending_rankHeadingTint[4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DBAB0 (D_800DBAB0@ENDING/ENDING.PRG)
CVECTOR Ending_rankLabelTint[8 / sizeof(CVECTOR)]; /* contents: module image (ModuleData.cpp) */

// decomp: _rank (_rank@ENDING/ENDING.PRG)
u_int Ending_riskbreakerRankIndex;

// decomp: _clearCount (_clearCount@ENDING/ENDING.PRG)
int Ending_clearCount;

// decomp: _score (_score@ENDING/ENDING.PRG)
u_int Ending_finalScore;

// decomp: _mapCompletion (_mapCompletion@ENDING/ENDING.PRG)
int Ending_mapCompletion;

// decomp: _onScreenMapCompletion (_onScreenMapCompletion@ENDING/ENDING.PRG)
u_int Ending_displayedMapCompletion;

// decomp: _onScreenScore (_onScreenScore@ENDING/ENDING.PRG)
int Ending_displayedScore;

// decomp: _submenuState (_submenuState@ENDING/ENDING.PRG)
int Ending_resultsLoadPhase;

// decomp: D_800DC20C (D_800DC20C@ENDING/ENDING.PRG)
int Ending_resultsAnimationFrame;

// decomp: D_800DC210 (D_800DC210@ENDING/ENDING.PRG)
int Ending_isLeavingResults;

// decomp: _incrementingScore (_incrementingScore@ENDING/ENDING.PRG)
int Ending_isScoreIncrementing;

// decomp: _incrementingMapCompletion (_incrementingMapCompletion@ENDING/ENDING.PRG)
int Ending_isMapCompletionIncrementing;

}  // extern "C"
