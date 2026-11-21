#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Menu/ManualPage.hpp"

// Bindings from the manual page cores (ManualPage.hpp) to the help text,
// the back buffer's draw environment, the page arrow counter, last frame's
// buttons and the current frame's primitive buffer, keeping the original
// call shapes for MENUE.PRG's callers. A help text outside a live game heap
// payload, an unknown gradient or arrow, an oversized control box, a
// missing ordering-table entry or a primitive buffer without room for the
// whole call is refused with a throttled warning, drawing nothing.
namespace VS::Menu::manual {
// decomp: MENUE.PRG drawContent (src/MENU/MENUE.PRG/494.c:581; was
// _drawContent).
void LiveDrawPageContent();
// decomp: MENUE.PRG setPageBg (src/MENU/MENUE.PRG/494.c:720; was _setPageBg).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawPageBackground(int x, int y, int w, int h, int color);
// decomp: MENUE.PRG fadeMenuUpper (src/MENU/MENUE.PRG/494.c:744; was
// _fadeMenuUpper).
void LiveDrawUpperScrollFade();
// decomp: MENUE.PRG fadeMenuLower (src/MENU/MENUE.PRG/494.c:795; was
// _fadeMenuLower).
void LiveDrawLowerScrollFade();
// decomp: MENUE.PRG drawPaginationArrow (src/MENU/MENUE.PRG/494.c:848; was
// _drawPaginationArrow).
void LiveDrawPageArrow(PageArrow arrow);
// decomp: MENUE.PRG drawControlsBg (src/MENU/MENUE.PRG/494.c:966; was
// _drawControlsBg). NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the
// original order.
void LiveDrawControlsBackground(int x, int y, int w, int h);
// Refusals logged since start-up.
[[nodiscard]] std::uint64_t LivePageRefusalCount();
}  // namespace VS::Menu::manual
