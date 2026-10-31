// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "DiscQueue.h"
#include "Ending/Results.h"
#include "Render/Images.h"

// decomp: _countableRooms (_countableRooms@ENDING/ENDING.PRG)
int Ending_countableRoomBits[16]; /* contents: module image (ModuleData.cpp) */

// decomp: _disMap (_disMap@ENDING/ENDING.PRG)
Render_TextureRegion Ending_resultAtlasRegions[69]; /* contents: module image (ModuleData.cpp) */

// decomp: _riskbreakerRanks (_riskbreakerRanks@ENDING/ENDING.PRG)
u_char Ending_rankLabelRegionIds[31][4]; /* contents: module image (ModuleData.cpp) */

// decomp: titleThresholds (titleThresholds@ENDING/ENDING.PRG)
short Ending_rankTitleThresholds[16]; /* contents: module image (ModuleData.cpp) */

// decomp: rankScores (rankScores@ENDING/ENDING.PRG)
u_int Ending_rankScoreThresholds[36]; /* contents: module image (ModuleData.cpp) */

// decomp: cdSlot (cdSlot@ENDING/ENDING.PRG)
Content_DiscQueueSlot* Ending_resultsDiscReadSlot;

// decomp: timData (timData@ENDING/ENDING.PRG)
void* Ending_resultsTimBuffer;

}  // extern "C"
