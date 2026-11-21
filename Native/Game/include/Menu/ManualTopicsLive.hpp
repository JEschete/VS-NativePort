#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

// Bindings from the manual topic and page text cores (ManualTopics.hpp) to
// the topic string table, the menu framework's rows and cursor memory, the
// loaded help text file and text box 0, keeping the original call shapes for
// MENUE.PRG's callers. Strings outside the topic table, help text outside a
// live game heap payload, and lines or pages that run past their bounds are
// refused with a throttled warning.
namespace VS::Menu::manual {
// decomp: MENUE.PRG setMenuRows (src/MENU/MENUE.PRG/494.c:82; was
// _setMenuRows). `strings` holds two pointers per row into the topic string
// table (_menuText).
void LiveSetTopicRows(int row_count, int highlighted_row, char** strings);
// decomp: MENUE.PRG getSelectedRow (src/MENU/MENUE.PRG/494.c:110; was
// _getSelectedRow).
int LiveGetSelectedTopicRow();
// decomp: MENUE.PRG topMenu (src/MENU/MENUE.PRG/494.c:454; was _topMenu). A
// topic table that cannot be listed is refused and reported as a cancel (-2),
// so the manual returns to the menu instead of polling rows that were never
// created.
int LiveUpdateTopicMenu(int init);
// decomp: MENUE.PRG getRoundedLineCount (src/MENU/MENUE.PRG/494.c:518; was
// _getRoundedLineCount). Returns 0 after a refused help text.
int LiveLastPageLineOffset();
// decomp: MENUE.PRG copyNextPageText (src/MENU/MENUE.PRG/494.c:535; was
// _copyNextPageText). A refused page leaves text box 0 untouched.
void LiveCopyPageText();
// Refusals logged since start-up.
[[nodiscard]] std::uint64_t LiveTopicRefusalCount();
}  // namespace VS::Menu::manual
