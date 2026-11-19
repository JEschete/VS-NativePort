#pragma once
// Hand-maintained (tools/port/owned.csv).
// The original MENUF.PRG cube-puzzle text, rank and time draw signatures bound
// to kDisMap, the par time, the puzzle clock of
// vs_main_stateFlags and the live atlas and time draws. A rank title outside
// the atlas draws nothing and logs a throttled warning.

namespace VS::Menu::Results {
void LiveDrawGivingUp(int draw_y, int fade_timer);
void LiveDrawChicken(int draw_y, int fade_timer);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawEvolveOrDie(int x, int y, int step);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawFoodChain(int x, int y, int timer);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawAverageTimeHeading(int x, int y, int timer);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawCubePuzzleParTime(int x, int y, int timer);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawCubePuzzleRank(int x, int y, int timer, int rank);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawCountingCubePuzzleTime(int x, int y, int timer);
}  // namespace VS::Menu::Results
