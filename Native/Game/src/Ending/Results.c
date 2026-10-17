// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "DiscQueue.h"
#include "Ending/Results.h"
#include "Render/Images.h"

// decomp: _countableRooms (_countableRooms@ENDING/ENDING.PRG)
int Ending_countableRoomBits[16]; /* data: contents loaded in Step 5 */

// decomp: _disMap (_disMap@ENDING/ENDING.PRG)
Render_TextureRegion Ending_resultAtlasRegions[69]; /* data: contents loaded in Step 5 */

// decomp: _riskbreakerRanks (_riskbreakerRanks@ENDING/ENDING.PRG)
u_char Ending_rankLabelRegionIds[31][4]; /* data: contents loaded in Step 5 */

// decomp: titleThresholds (titleThresholds@ENDING/ENDING.PRG)
short Ending_rankTitleThresholds[16]; /* data: contents loaded in Step 5 */

// decomp: rankScores (rankScores@ENDING/ENDING.PRG)
u_int Ending_rankScoreThresholds[36]; /* data: contents loaded in Step 5 */

// decomp: cdSlot (cdSlot@ENDING/ENDING.PRG)
Content_DiscQueueSlot* Ending_resultsDiscReadSlot;

// decomp: timData (timData@ENDING/ENDING.PRG)
void* Ending_resultsTimBuffer;

