#pragma once
// Hand-maintained (tools/port/owned.csv).
// The original MENU9.PRG score screen signatures (F04157, F02833, F02835,
// F02832) bound to vs_main_scoredata, vs_main_stateFlags, the room visit
// flags, the action table and the score screen statics.

#include "Menu/Score/ScoreRank.hpp"

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
// Fresh value views of the live score record and title conditions.
[[nodiscard]] ScoreRecord ReadLiveScoreRecord();
[[nodiscard]] TitleConditions ReadLiveTitleConditions();

// _setTitleFlags: ors the earned titles into vs_main_scoredata.titles.
void AwardLiveTitles();
// _calculateScore: stores the score in _score.
void CalculateLiveScore();
// _determineCharacterRank: stores the rank in _characterRank (kept when no
// row matches).
void DetermineLiveRiskbreakerRank();
// _calculateProgress: always returns 1.
int CalculateLiveProgress();
}  // namespace VS::Menu::Score
