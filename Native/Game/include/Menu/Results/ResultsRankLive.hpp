#pragma once
// Hand-maintained (tools/port/owned.csv).
// The original MENUF.PRG results screen signatures (F03061, F03060) bound to
// vs_main_scoredata and the results screen statics.

namespace VS::Menu::Results {
// _calculateScore: stores the score in _score.
void CalculateLiveScore();
// _determineRank: stores the rank in _rank (kept when no row matches).
void DetermineLiveRiskbreakerRank();
}  // namespace VS::Menu::Results
