// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Ending/ClearResults.hpp"

// decomp: D_800DBA7C (D_800DBA7C@ENDING/ENDING.PRG)
VS::Render::RegionColor VS::Ending::congratulationsTint[VS::Ending::kCongratulationsTints]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DBA88 (D_800DBA88@ENDING/ENDING.PRG)
VS::Ending::RegionColorPair VS::Ending::scoreTint; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DBA90 (D_800DBA90@ENDING/ENDING.PRG)
VS::Ending::RegionColorPair VS::Ending::countingScoreTint; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DBA98 (D_800DBA98@ENDING/ENDING.PRG)
VS::Ending::RegionColorPair VS::Ending::mapCompletionTint; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DBAA0 (D_800DBAA0@ENDING/ENDING.PRG)
VS::Ending::RegionColorPair VS::Ending::countingMapCompletionTint; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DBAA8 (D_800DBAA8@ENDING/ENDING.PRG)
VS::Ending::RegionColorPair VS::Ending::rankHeadingTint; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DBAB0 (D_800DBAB0@ENDING/ENDING.PRG)
VS::Ending::RegionColorPair VS::Ending::rankLabelTint; /* contents: module image (ModuleData.cpp) */

// decomp: _rank (_rank@ENDING/ENDING.PRG)
std::uint32_t VS::Ending::riskbreakerRankIndex;

// decomp: _clearCount (_clearCount@ENDING/ENDING.PRG)
std::int32_t VS::Ending::clearCount;

// decomp: _score (_score@ENDING/ENDING.PRG)
std::uint32_t VS::Ending::finalScore;

// decomp: _mapCompletion (_mapCompletion@ENDING/ENDING.PRG)
std::int32_t VS::Ending::mapCompletion;

// decomp: _onScreenMapCompletion (_onScreenMapCompletion@ENDING/ENDING.PRG)
std::uint32_t VS::Ending::displayedMapCompletion;

// decomp: _onScreenScore (_onScreenScore@ENDING/ENDING.PRG)
std::int32_t VS::Ending::displayedScore;

// decomp: _submenuState (_submenuState@ENDING/ENDING.PRG)
std::int32_t VS::Ending::resultsLoadPhase;

// decomp: D_800DC20C (D_800DC20C@ENDING/ENDING.PRG)
std::int32_t VS::Ending::resultsAnimationFrame;

// decomp: D_800DC210 (D_800DC210@ENDING/ENDING.PRG)
std::int32_t VS::Ending::isLeavingResults;

// decomp: _incrementingScore (_incrementingScore@ENDING/ENDING.PRG)
std::int32_t VS::Ending::isScoreIncrementing;

// decomp: _incrementingMapCompletion (_incrementingMapCompletion@ENDING/ENDING.PRG)
std::int32_t VS::Ending::isMapCompletionIncrementing;

