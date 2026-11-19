// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Ending/Results.hpp"

// decomp: _countableRooms (_countableRooms@ENDING/ENDING.PRG)
std::int32_t VS::Ending::countableRoomBits[VS::Ending::kRoomFlagWords]; /* contents: module image (ModuleData.cpp) */

// decomp: _disMap (_disMap@ENDING/ENDING.PRG)
VS::Render::TextureRegion VS::Ending::resultAtlasRegions[VS::Ending::kResultAtlasRegions]; /* contents: module image (ModuleData.cpp) */

// decomp: _riskbreakerRanks (_riskbreakerRanks@ENDING/ENDING.PRG)
VS::Ending::RankLabel VS::Ending::rankLabelRegionIds[VS::Ending::kRankLabelEntries]; /* contents: module image (ModuleData.cpp) */

// decomp: titleThresholds (titleThresholds@ENDING/ENDING.PRG)
std::int16_t VS::Ending::rankTitleThresholds[VS::Ending::kRankCount]; /* contents: module image (ModuleData.cpp) */

// decomp: rankScores (rankScores@ENDING/ENDING.PRG)
std::uint32_t VS::Ending::rankScoreThresholds[VS::Ending::kRankScoreThresholds]; /* contents: module image (ModuleData.cpp) */

// decomp: cdSlot (cdSlot@ENDING/ENDING.PRG)
Content_DiscQueueSlot* VS::Ending::resultsDiscReadSlot;

// decomp: timData (timData@ENDING/ENDING.PRG)
void* VS::Ending::resultsTimBuffer;

