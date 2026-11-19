#pragma once
// ENDING.PRG's clear results: the draws' color pairs from the module image,
// and the results screen's score, rank and count-up state, which EndingLive.cpp
// publishes from the core's ResultsState.
#include <cstddef>
#include <cstdint>

#include "Ending/ClearResultsCore.hpp"
#include "Render/TextureRegionDraw.hpp"

namespace VS::Ending {
// Each draw reads its table's first two colors; the congratulations pop-in
// also draws from the second entry (D4.c:1970), so its table holds three.
// decomp: D_800DBA7C (D_800DBA7C@ENDING/ENDING.PRG)
extern Render::RegionColor congratulationsTint[kCongratulationsTints];
// decomp: D_800DBA88 (D_800DBA88@ENDING/ENDING.PRG)
extern RegionColorPair scoreTint;
// decomp: D_800DBA90 (D_800DBA90@ENDING/ENDING.PRG)
extern RegionColorPair countingScoreTint;
// decomp: D_800DBA98 (D_800DBA98@ENDING/ENDING.PRG)
extern RegionColorPair mapCompletionTint;
// decomp: D_800DBAA0 (D_800DBAA0@ENDING/ENDING.PRG)
extern RegionColorPair countingMapCompletionTint;
// decomp: D_800DBAA8 (D_800DBAA8@ENDING/ENDING.PRG)
extern RegionColorPair rankHeadingTint;
// decomp: D_800DBAB0 (D_800DBAB0@ENDING/ENDING.PRG)
extern RegionColorPair rankLabelTint;
// decomp: _rank (_rank@ENDING/ENDING.PRG)
extern std::uint32_t riskbreakerRankIndex;
// decomp: _clearCount (_clearCount@ENDING/ENDING.PRG)
extern std::int32_t clearCount;
// decomp: _score (_score@ENDING/ENDING.PRG)
extern std::uint32_t finalScore;
// decomp: _mapCompletion (_mapCompletion@ENDING/ENDING.PRG)
extern std::int32_t mapCompletion;
// decomp: _onScreenMapCompletion (_onScreenMapCompletion@ENDING/ENDING.PRG)
extern std::uint32_t displayedMapCompletion;
// decomp: _onScreenScore (_onScreenScore@ENDING/ENDING.PRG)
extern std::int32_t displayedScore;
// decomp: _submenuState (_submenuState@ENDING/ENDING.PRG)
extern std::int32_t resultsLoadPhase;
// decomp: D_800DC20C (D_800DC20C@ENDING/ENDING.PRG)
extern std::int32_t resultsAnimationFrame;
// decomp: D_800DC210 (D_800DC210@ENDING/ENDING.PRG)
extern std::int32_t isLeavingResults;
// decomp: _incrementingScore (_incrementingScore@ENDING/ENDING.PRG)
extern std::int32_t isScoreIncrementing;
// decomp: _incrementingMapCompletion
// (_incrementingMapCompletion@ENDING/ENDING.PRG)
extern std::int32_t isMapCompletionIncrementing;
}  // namespace VS::Ending
