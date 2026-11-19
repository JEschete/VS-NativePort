#pragma once
// The MENUF.PRG results palette and the cube puzzle's rating
// (src/MENU/MENUF.PRG/3B8.c).
#include <cstdint>

#include "Menu/Results/ResultsPalette.hpp"

namespace VS::Menu::Results {
// decomp: _clutBuf (_clutBuf@src/MENU/MENUF.PRG/3B8.c): the five palette rows
// the results screens pulse and upload (ResultsPalette.hpp).
extern std::uint16_t clutBuf[kResultsClutWords];
// decomp: _puzzleParTime (_puzzleParTime@src/MENU/MENUF.PRG/3B8.c): the
// finished puzzle's par time in seconds (CubePuzzleRating::par_seconds).
extern std::int16_t puzzleParTime;
// decomp: _puzzleRankCap (_puzzleRankCap@src/MENU/MENUF.PRG/3B8.c): the
// finished puzzle's IQ.DIS rank cap (CubePuzzleRating::rank_cap).
extern std::int16_t puzzleRankCap;
}  // namespace VS::Menu::Results
