#pragma once
// ENDING.PRG's results tables from the module image (the countable rooms, the
// atlas regions, the rank labels and thresholds) and the results screen's
// disc read.
#include <cstddef>
#include <cstdint>

#include "Ending/ClearResultsCore.hpp"
#include "Render/Images.hpp"

namespace VS::Content {
struct DiscQueueSlot;
}  // namespace VS::Content

namespace VS::Ending {
inline constexpr std::size_t kResultAtlasRegions = 69;
inline constexpr std::size_t kRankLabelEntries = 31;
inline constexpr std::size_t kRankScoreThresholds = 36;

// decomp: _countableRooms (_countableRooms@ENDING/ENDING.PRG)
extern std::int32_t countableRoomBits[kRoomFlagWords];
// decomp: _disMap (_disMap@ENDING/ENDING.PRG)
// Region 12's width is also read alone as D_800DB876, the rank heading's
// width (ClearResultsCore's ResultsTables::rank_heading_width).
extern Render::TextureRegion resultAtlasRegions[kResultAtlasRegions];
// decomp: _riskbreakerRanks (_riskbreakerRanks@ENDING/ENDING.PRG)
static_assert(sizeof(RankLabel) == kRankLabelRegions + 1);
extern RankLabel rankLabelRegionIds[kRankLabelEntries];
// decomp: titleThresholds (titleThresholds@ENDING/ENDING.PRG)
extern std::int16_t rankTitleThresholds[kRankCount];
// decomp: rankScores (rankScores@ENDING/ENDING.PRG)
extern std::uint32_t rankScoreThresholds[kRankScoreThresholds];
// decomp: cdSlot (cdSlot@ENDING/ENDING.PRG)
extern VS::Content::DiscQueueSlot* resultsDiscReadSlot;
// decomp: timData (timData@ENDING/ENDING.PRG)
extern void* resultsTimBuffer;
}  // namespace VS::Ending
