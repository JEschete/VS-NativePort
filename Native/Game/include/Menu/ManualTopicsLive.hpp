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
// decomp: MENUE.PRG _setMenuRows (494.c:76). `strings` holds two pointers
// per row into the topic string table (_menuText).
void LiveSetTopicRows(int row_count, int highlighted_row, char** strings);
// decomp: MENUE.PRG _getSelectedRow (494.c:104).
int LiveGetSelectedTopicRow();
// decomp: MENUE.PRG _topMenu (494.c:450). A topic table that cannot be
// listed is refused and reported as a cancel (-2), so the manual returns to
// the menu instead of polling rows that were never created.
int LiveUpdateTopicMenu(int init);
// decomp: MENUE.PRG _getRoundedLineCount (494.c:514). Returns 0 after a
// refused help text.
int LiveLastPageLineOffset();
// decomp: MENUE.PRG _copyNextPageText (494.c:531). A refused page leaves
// text box 0 untouched.
void LiveCopyPageText();
// Refusals logged since start-up.
[[nodiscard]] std::uint64_t LiveTopicRefusalCount();
}  // namespace VS::Menu::manual
