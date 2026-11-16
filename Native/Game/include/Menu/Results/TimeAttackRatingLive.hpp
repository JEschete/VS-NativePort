#pragma once
// Hand-maintained (tools/port/owned.csv).
// The original MENUF.PRG time-attack results signatures bound to
// Menu_Results_disMap, the completed time (_timeTrialTime), the new record's
// rank (_newTimeSlot), the time-attack course and its trial scores and the
// live atlas draws. A course past the eight trial-score rows or a leaderboard
// slot outside the three ranks draws nothing and logs a throttled warning.

namespace VS::Menu::Results {
void LiveDrawTimeAttackResultsHeading(int x, int y, int timer);
void LiveDrawTimeAttackRating(int x, int y, int fade_timer);
void LiveDrawTimeAttackRatingWipe(int x, int y, int timer);
void LiveDrawTimeAttackTime(int x, int y, int fade_timer);
void LiveDrawCountingTimeAttackTime(int x, int y, int timer);
void LiveDrawNewRecord(int x, int y, int timer);
void LiveDrawLeaderboard(int x, int y, int timer);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawLeaderboardEntry(int x, int y, int slot, int timer);
}  // namespace VS::Menu::Results
