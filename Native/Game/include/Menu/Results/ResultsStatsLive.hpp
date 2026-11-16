#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <optional>

#include "Menu/Results/ResultsStats.hpp"

namespace VS::Menu::Results {
// Fresh value view of Battle_characterState->stats; nothing (after a
// warning) when the character or its stat record is missing.
[[nodiscard]] std::optional<AshleyResultStats> ReadLiveAshleyResultStats();
// Stores the view back; refuses with a warning when the record is missing.
void WriteLiveAshleyResultStats(const AshleyResultStats& stats);

// The original MENUF.PRG signatures bound to Battle_characterState. A missing
// character or stat record logs and refuses: the reads return 0 and the
// bonuses write nothing.
int AshleyLiveTotalStrength();
int AshleyLiveTotalIntelligence();
int AshleyLiveTotalAgility();
void ApplyLiveStrengthBonus(int amount);
void ApplyLiveIntelligenceBonus(int amount);
void ApplyLiveAgilityBonus(int amount);
void ApplyLiveHpBonus(int amount);
void ApplyLiveMpBonus(int amount);
}  // namespace VS::Menu::Results
