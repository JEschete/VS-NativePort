// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Ending/Results.hpp"

// decomp: _countableRooms (_countableRooms@ENDING/ENDING.PRG)
// contents: module image (ModuleData.cpp)
std::int32_t VS::Ending::countableRoomBits[VS::Ending::kRoomFlagWords];

// decomp: _disMap (_disMap@ENDING/ENDING.PRG)
// contents: module image (ModuleData.cpp)
VS::Render::TextureRegion
    VS::Ending::resultAtlasRegions[VS::Ending::kResultAtlasRegions];

// decomp: _riskbreakerRanks (_riskbreakerRanks@ENDING/ENDING.PRG)
// contents: module image (ModuleData.cpp)
VS::Ending::RankLabel
    VS::Ending::rankLabelRegionIds[VS::Ending::kRankLabelEntries];

// decomp: titleThresholds (titleThresholds@ENDING/ENDING.PRG)
// contents: module image (ModuleData.cpp)
std::int16_t VS::Ending::rankTitleThresholds[VS::Ending::kRankCount];

// decomp: rankScores (rankScores@ENDING/ENDING.PRG)
// contents: module image (ModuleData.cpp)
std::uint32_t VS::Ending::rankScoreThresholds[VS::Ending::kRankScoreThresholds];

// decomp: cdSlot (cdSlot@ENDING/ENDING.PRG)
VS::Content::DiscQueueSlot* VS::Ending::resultsDiscReadSlot;

// decomp: timData (timData@ENDING/ENDING.PRG)
void* VS::Ending::resultsTimBuffer;
