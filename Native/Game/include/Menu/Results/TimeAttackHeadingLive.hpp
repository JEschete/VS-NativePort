#pragma once
// Hand-maintained (tools/port/owned.csv).
// The original MENUF.PRG time-attack heading, countdown and palette pulse
// signatures bound to Menu_Results_disMap, the time-attack course, its best
// times, Menu_Results_clutBuf and the live atlas draws. A course digit, boss
// name or countdown number outside the atlas, or a course without a best time,
// draws nothing and logs a throttled warning.

namespace VS::Menu::Results {
void LiveDrawTimeAttackHeading(int x, int y, int timer);
void LiveDrawCourseNumber(int x, int y, int timer);
void LiveDrawBossName(int x, int y, int timer);
void LiveDrawBestTime(int x, int y, int timer);
void LiveDrawStartCountdown(int x, int y, int timer);
void LivePulseTimeAttackClut(int blend_weight);
}  // namespace VS::Menu::Results
