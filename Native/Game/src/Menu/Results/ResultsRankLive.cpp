// Hand-maintained (tools/port/owned.csv).
// Binds the shared score and rank policies (Menu/Score/ScoreRank.cpp) to the
// MENUF.PRG results screen statics the generated functions used.
#include "Menu/Results/ResultsRankLive.hpp"

#include <cstdint>

#include "Legacy/Psx.h"
#include "Menu/Results/Results.h"
#include "Menu/Score/ScoreRank.hpp"
#include "Menu/Score/ScoreRankLive.hpp"
#include "State/Progress.h"

namespace VS::Menu::Results {
// decomp: MENUF.PRG _calculateScore (src/MENU/MENUF.PRG/3B8.c:2801). The int
// clear count (lw) is the 32-bit value ScoreFromRecord multiplies.
void CalculateLiveScore() {
  Menu_Results_score = Score::ScoreFromRecord(
      Score::ReadLiveScoreRecord(),
      static_cast<std::uint32_t>(Menu_Results_clearCount));
}

// decomp: MENUF.PRG _determineRank (src/MENU/MENUF.PRG/3B8.c:2774). The rank
// row index is stored as the original's 32-bit int (sw); no matching row keeps
// the previous rank.
void DetermineLiveRiskbreakerRank() {
  const auto rank = Score::RiskbreakerRankIndex(
      static_cast<std::uint32_t>(VS::State::scoreData.titles),
      Menu_Results_score);
  if (rank.has_value()) {
    Menu_Results_rank = static_cast<std::int32_t>(*rank);
  }
}
}  // namespace VS::Menu::Results
