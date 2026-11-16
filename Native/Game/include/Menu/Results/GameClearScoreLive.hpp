#pragma once
// Hand-maintained (tools/port/owned.csv).
// The original MENUF.PRG game-clear draw signatures bound to
// Menu_Results_disMap, the results score, map completion and rank statics, the
// counting readouts and the live atlas draws and sound effects. A value that
// prints negative, a rank outside the table or a rank word outside the atlas
// draws nothing and logs a throttled warning.

namespace VS::Menu::Results {
void LiveDrawCongratulations(int x, int y, int timer);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawScore(int x, int y, int timer, int unused);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawCountingScore(int x, int y, int timer, int unused);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawMapCompletion(int x, int y, int timer, int unused);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawCountingMapCompletion(int x, int y, int timer, int unused);
void LiveDrawRiskbreakerRankHeading(int x, int y, int timer);
void LiveDrawRiskbreakerRank(int x, int y, int timer);
}  // namespace VS::Menu::Results
