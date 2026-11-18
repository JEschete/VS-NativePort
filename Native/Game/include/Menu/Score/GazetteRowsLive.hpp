#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

struct Menu_Framework_MenuRow;

// Binds the gazette rows (GazetteRows.hpp) to the frame's primitive buffer
// and overlay ordering table (Render_frameScratch), vs_main_frameBuf
// (Render_frameBuf), the menu framework's rows and cursor memory
// (VS::Menu::Framework::menuMemory), the cursor memory setting
// (Input_settings), the gazette's remembered row
// (Battle_unusedMenu9CursorMemory) and the top menu strings
// (Menu_Score_menuText), in the shapes the copied score screen calls. The arrow
// animation and the top menu's step live here instead of 238.c's statics. A
// null row, text or ordering table, a row count outside 0-256, a full primitive
// buffer and row text without an end are refused and reported on the
// "menu.score" channel.
namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
struct ScrollableRowAnimation;
struct TopMenuState;

// MENU9.PRG (238.c).
void RenderLiveScrollableRow(Menu_Framework_MenuRow* row);
void RestoreLiveTopMenuCursor(int row_count, int absolute_row, char* text[]);
int LiveSelectedTopMenuRow();
int UpdateLiveTopMenu(int initialize);
void ClearLiveMenuExceptRow8();
void PollLiveMenuReady();

// Call where MENU9.PRG loads (Boot_ResetMenu9Statics): its reload zeroes the
// arrow animation and the top menu's step (238.c's statics are bss).
void ResetGazetteRowsLive();
// The bound state, for the reset and tests.
[[nodiscard]] ScrollableRowAnimation& LiveScrollableRowAnimation() noexcept;
[[nodiscard]] TopMenuState& LiveTopMenuState() noexcept;
// Inputs the binding refused since start-up.
[[nodiscard]] std::uint64_t LiveGazetteRowsRefusals() noexcept;
}  // namespace VS::Menu::Score
