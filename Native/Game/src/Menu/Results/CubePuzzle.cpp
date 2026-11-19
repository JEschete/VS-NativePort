// The MENUF.PRG results palette and the cube puzzle's rating.
#include "Menu/Results/CubePuzzle.hpp"

#include <cstdint>

#include "Menu/Results/ResultsPalette.hpp"

namespace VS::Menu::Results {
// decomp: _clutBuf (_clutBuf@src/MENU/MENUF.PRG/3B8.c)
std::uint16_t clutBuf[kResultsClutWords];

// decomp: _puzzleParTime (_puzzleParTime@src/MENU/MENUF.PRG/3B8.c)
std::int16_t puzzleParTime;

// decomp: _puzzleRankCap (_puzzleRankCap@src/MENU/MENUF.PRG/3B8.c)
std::int16_t puzzleRankCap;
}  // namespace VS::Menu::Results
